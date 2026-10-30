package com.remotecontrol.relay.registry;

import io.netty.channel.Channel;
import io.netty.channel.ChannelId;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;
import java.util.function.LongSupplier;
import java.util.Objects;
import java.util.HashSet;
import java.util.Set;
import java.time.Duration;
import io.netty.util.concurrent.ScheduledFuture;

/** Owns CHILD registrations and the process-local MVP Relay sessions. */
public final class RelayRegistry {

    private final ConcurrentMap<String, Channel> childChannels = new ConcurrentHashMap<>();
    private final ConcurrentMap<ChannelId, String> agentSessionIds = new ConcurrentHashMap<>();
    private final ConcurrentMap<Long, SessionRecord> sessions = new ConcurrentHashMap<>();
    private final ConcurrentMap<ChannelId, Long> adminSessions = new ConcurrentHashMap<>();
    private final ConcurrentMap<ChannelId, Long> childSessions = new ConcurrentHashMap<>();
    private final Set<ChannelId> closingChannels = new HashSet<>();
    private long nextSessionId = 1L;
    private static final java.time.Duration DEFAULT_SESSION_TIMEOUT = java.time.Duration.ofSeconds(15);

    private final long timeoutNanos;
    private final LongSupplier clock;
    private final Consumer<SessionRecord> timeoutCallback;

    public RelayRegistry() {
        this(session -> {});
    }

    public RelayRegistry(Consumer<SessionRecord> timeoutCallback) {
        this(DEFAULT_SESSION_TIMEOUT, System::nanoTime, timeoutCallback);
    }

    public RelayRegistry(Duration timeout, LongSupplier clock, Consumer<SessionRecord> timeoutCallback) {
        this.timeoutNanos = Objects.requireNonNull(timeout).toNanos();
        if (timeoutNanos <= 0) throw new IllegalArgumentException("timeout must be positive");
        this.clock = Objects.requireNonNull(clock);
        this.timeoutCallback = Objects.requireNonNull(timeoutCallback);
    }

    private enum SessionState {
        PENDING,
        ACTIVE
    }

    public static final class SessionRecord {
        private final long sessionId;
        private final Channel adminChannel;
        private final Channel childChannel;
        private final String agentSessionId;
        private SessionState state;
        private long deadlineNanos;
        private ScheduledFuture<?> timeoutTask;

        private SessionRecord(long sessionId, Channel adminChannel,
                              Channel childChannel, String agentSessionId) {
            this.sessionId = sessionId;
            this.adminChannel = adminChannel;
            this.childChannel = childChannel;
            this.agentSessionId = agentSessionId;
            this.state = SessionState.PENDING;
        }

        public long getSessionId() {
            return sessionId;
        }

        public Channel getChildChannel() {
            return childChannel;
        }

        public Channel getAdminChannel() {
            return adminChannel;
        }
    }

    /**
     * Registers a CHILD channel. Repeating the same agentSessionId on the same channel
     * is accepted idempotently; all other duplicate agentSessionId/channel bindings
     * are rejected without replacing the existing registration.
     */
    public synchronized boolean registerChild(String agentSessionId, Channel channel) {
        if (closingChannels.contains(channel.id()) || !channel.isActive()) return false;
        String existingSessionId = agentSessionIds.putIfAbsent(channel.id(), agentSessionId);
        if (existingSessionId != null) {
            return existingSessionId.equals(agentSessionId)
                    && childChannels.get(agentSessionId) == channel;
        }

        Channel existingChannel = childChannels.putIfAbsent(agentSessionId, channel);
        if (existingChannel == null || existingChannel == channel) {
            return true;
        }

        agentSessionIds.remove(channel.id(), agentSessionId);
        return false;
    }

    /** Removes only the binding still owned by the closing channel. */
    public synchronized void unregisterChild(Channel channel) {
        String agentSessionId = agentSessionIds.remove(channel.id());
        if (agentSessionId != null) {
            childChannels.remove(agentSessionId, channel);
        }
    }

    public Channel findRegisteredChild(String agentSessionId) {
        return childChannels.get(agentSessionId);
    }

    public boolean isRegisteredChild(Channel channel) {
        return agentSessionIds.containsKey(channel.id());
    }

    public boolean hasSession(Channel channel) {
        return adminSessions.containsKey(channel.id()) || childSessions.containsKey(channel.id());
    }

    public synchronized SessionRecord createPendingSession(
            Channel adminChannel, Channel childChannel, String agentSessionId) {
        if (closingChannels.contains(adminChannel.id()) || closingChannels.contains(childChannel.id())
                || isRegisteredChild(adminChannel)
                || !adminChannel.isActive()
                || !childChannel.isActive()
                || childChannels.get(agentSessionId) != childChannel
                || adminSessions.containsKey(adminChannel.id())
                || childSessions.containsKey(childChannel.id())) {
            return null;
        }

        long sessionId = allocateSessionId();
        SessionRecord session = new SessionRecord(
                sessionId, adminChannel, childChannel, agentSessionId);
        session.deadlineNanos = clock.getAsLong() + timeoutNanos;
        sessions.put(sessionId, session);
        adminSessions.put(adminChannel.id(), sessionId);
        childSessions.put(childChannel.id(), sessionId);
        session.timeoutTask = adminChannel.eventLoop().schedule(
                () -> expirePendingSession(session), timeoutNanos, TimeUnit.NANOSECONDS);
        return session;
    }

    public SessionRecord acceptPendingSession(long sessionId, Channel childChannel) {
        SessionRecord expired = null;
        synchronized (this) {
            SessionRecord session = sessions.get(sessionId);
            if (session == null || session.state != SessionState.PENDING
                    || session.childChannel != childChannel) return null;
            if (clock.getAsLong() - session.deadlineNanos >= 0) {
                removeSession(session);
                retireChild(session.childChannel);
                expired = session;
            } else {
                session.state = SessionState.ACTIVE;
                cancelTimeout(session);
                return session;
            }
        }
        timeoutCallback.accept(expired);
        return null;
    }

    private void expirePendingSession(SessionRecord session) {
        synchronized (this) {
            if (sessions.get(session.sessionId) != session || session.state != SessionState.PENDING) return;
            long remaining = session.deadlineNanos - clock.getAsLong();
            if (remaining > 0) {
                session.timeoutTask = session.adminChannel.eventLoop().schedule(
                        () -> expirePendingSession(session), remaining, TimeUnit.NANOSECONDS);
                return;
            }
            removeSession(session);
            retireChild(session.childChannel);
        }
        timeoutCallback.accept(session);
    }

    public SessionRecord rejectPendingSession(long sessionId, Channel childChannel) {
        SessionRecord expired;
        synchronized (this) {
            SessionRecord session = sessions.get(sessionId);
            if (session == null || session.state != SessionState.PENDING
                    || session.childChannel != childChannel) return null;
            if (clock.getAsLong() - session.deadlineNanos < 0) {
                removeSession(session);
                return session;
            }
            removeSession(session);
            retireChild(session.childChannel);
            expired = session;
        }
        timeoutCallback.accept(expired);
        return null;
    }

    // A channel selected for closure cannot be reused while the close is pending.
    private void reserveClosing(Channel channel) {
        if (closingChannels.add(channel.id())) {
            channel.closeFuture().addListener(future -> {
                synchronized (RelayRegistry.this) {
                    closingChannels.remove(channel.id());
                }
            });
        }
    }

    private void retireChild(Channel child) {
        reserveClosing(child);
        unregisterChild(child);
    }

    public synchronized SessionRecord findActiveSessionForChild(
            long sessionId, Channel childChannel) {
        SessionRecord session = sessions.get(sessionId);
        if (sessionId == 0
                || session == null
                || session.state != SessionState.ACTIVE
                || session.childChannel != childChannel) {
            return null;
        }

        return session;
    }

    public void removeSessionForChannel(Channel channel) {
        Channel peer;
        synchronized (this) {
        Long sessionId = adminSessions.get(channel.id());
        if (sessionId == null) {
            sessionId = childSessions.get(channel.id());
        }
        if (sessionId == null) {
            return;
        }

        SessionRecord session = sessions.get(sessionId);
        if (session != null) {
            removeSession(session);
            peer = session.adminChannel == channel ? session.childChannel : session.adminChannel;
            reserveClosing(peer);
        } else return;
        }
        peer.close(); // Both clients reset their transport/session state.
    }

    public record ForwardTarget(Channel channel, boolean fromChild) {}

    public synchronized ForwardTarget findActivePeer(long sessionId, Channel sourceChannel) {
        SessionRecord session = sessions.get(sessionId);
        if (sessionId == 0 || session == null || session.state != SessionState.ACTIVE
                || !session.adminChannel.isActive() || !session.childChannel.isActive()) return null;
        if (sourceChannel == session.childChannel) return new ForwardTarget(session.adminChannel, true);
        if (sourceChannel == session.adminChannel) return new ForwardTarget(session.childChannel, false);
        return null;
    }

    public int registeredChildCount() {
        return (int) childChannels.values().stream().filter(Channel::isActive).count();
    }

    public synchronized int activeSessionCount() {
        return (int) sessions.values().stream().filter(s -> s.state == SessionState.ACTIVE
                && s.adminChannel.isActive() && s.childChannel.isActive()).count();
    }

    private long allocateSessionId() {
        if (nextSessionId <= 0) {
            throw new IllegalStateException("Relay session ID space exhausted");
        }

        long allocated = nextSessionId;
        nextSessionId = nextSessionId == Long.MAX_VALUE ? 0 : nextSessionId + 1;
        return allocated;
    }

    public synchronized int pendingSessionCount() {
        return (int) sessions.values().stream().filter(s -> s.state == SessionState.PENDING).count();
    }

    private void cancelTimeout(SessionRecord session) {
        if (session.timeoutTask != null) {
            session.timeoutTask.cancel(false);
            session.timeoutTask = null;
        }
    }

    private void removeSession(SessionRecord session) {
        cancelTimeout(session);
        sessions.remove(session.sessionId, session);
        adminSessions.remove(session.adminChannel.id(), session.sessionId);
        childSessions.remove(session.childChannel.id(), session.sessionId);
    }
}
