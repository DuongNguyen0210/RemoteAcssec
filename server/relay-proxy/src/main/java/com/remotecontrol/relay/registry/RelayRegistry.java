package com.remotecontrol.relay.registry;

import io.netty.channel.Channel;
import io.netty.channel.ChannelId;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/** Owns CHILD registrations and the process-local MVP Relay sessions. */
public final class RelayRegistry {

    private final ConcurrentMap<String, Channel> childChannels = new ConcurrentHashMap<>();
    private final ConcurrentMap<ChannelId, String> childUsernames = new ConcurrentHashMap<>();
    private final ConcurrentMap<Long, SessionRecord> sessions = new ConcurrentHashMap<>();
    private final ConcurrentMap<ChannelId, Long> adminSessions = new ConcurrentHashMap<>();
    private final ConcurrentMap<ChannelId, Long> childSessions = new ConcurrentHashMap<>();
    private long nextSessionId = 1L;

    private enum SessionState {
        PENDING,
        ACTIVE
    }

    public static final class SessionRecord {
        private final long sessionId;
        private final Channel adminChannel;
        private final Channel childChannel;
        private final String childUsername;
        private SessionState state;

        private SessionRecord(long sessionId, Channel adminChannel,
                              Channel childChannel, String childUsername) {
            this.sessionId = sessionId;
            this.adminChannel = adminChannel;
            this.childChannel = childChannel;
            this.childUsername = childUsername;
            this.state = SessionState.PENDING;
        }

        public long getSessionId() {
            return sessionId;
        }

        public Channel getAdminChannel() {
            return adminChannel;
        }
    }

    /**
     * Registers a CHILD channel. Repeating the same username on the same channel
     * is accepted idempotently; all other duplicate username/channel bindings
     * are rejected without replacing the existing registration.
     */
    public boolean registerChild(String username, Channel channel) {
        String existingUsername = childUsernames.putIfAbsent(channel.id(), username);
        if (existingUsername != null) {
            return existingUsername.equals(username)
                    && childChannels.get(username) == channel;
        }

        Channel existingChannel = childChannels.putIfAbsent(username, channel);
        if (existingChannel == null || existingChannel == channel) {
            return true;
        }

        childUsernames.remove(channel.id(), username);
        return false;
    }

    /** Removes only the binding still owned by the closing channel. */
    public void unregisterChild(Channel channel) {
        String username = childUsernames.remove(channel.id());
        if (username != null) {
            childChannels.remove(username, channel);
        }
    }

    public Channel findRegisteredChild(String username) {
        return childChannels.get(username);
    }

    public boolean isRegisteredChild(Channel channel) {
        return childUsernames.containsKey(channel.id());
    }

    public synchronized SessionRecord createPendingSession(
            Channel adminChannel, Channel childChannel, String childUsername) {
        if (isRegisteredChild(adminChannel)
                || !adminChannel.isActive()
                || !childChannel.isActive()
                || childChannels.get(childUsername) != childChannel
                || adminSessions.containsKey(adminChannel.id())
                || childSessions.containsKey(childChannel.id())) {
            return null;
        }

        long sessionId = allocateSessionId();
        SessionRecord session = new SessionRecord(
                sessionId, adminChannel, childChannel, childUsername);
        sessions.put(sessionId, session);
        adminSessions.put(adminChannel.id(), sessionId);
        childSessions.put(childChannel.id(), sessionId);
        return session;
    }

    public synchronized SessionRecord acceptPendingSession(
            long sessionId, Channel childChannel) {
        SessionRecord session = sessions.get(sessionId);
        if (session == null
                || session.state != SessionState.PENDING
                || session.childChannel != childChannel) {
            return null;
        }

        session.state = SessionState.ACTIVE;
        return session;
    }

    public synchronized SessionRecord rejectPendingSession(
            long sessionId, Channel childChannel) {
        SessionRecord session = sessions.get(sessionId);
        if (session == null
                || session.state != SessionState.PENDING
                || session.childChannel != childChannel) {
            return null;
        }

        removeSession(session);
        return session;
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

    public synchronized void removeSessionForChannel(Channel channel) {
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
        }
    }

    private long allocateSessionId() {
        if (nextSessionId <= 0) {
            throw new IllegalStateException("Relay session ID space exhausted");
        }

        long allocated = nextSessionId;
        nextSessionId = nextSessionId == Long.MAX_VALUE ? 0 : nextSessionId + 1;
        return allocated;
    }

    private void removeSession(SessionRecord session) {
        sessions.remove(session.sessionId, session);
        adminSessions.remove(session.adminChannel.id(), session.sessionId);
        childSessions.remove(session.childChannel.id(), session.sessionId);
    }
}
