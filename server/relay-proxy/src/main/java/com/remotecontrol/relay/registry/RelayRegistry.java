package com.remotecontrol.relay.registry;

import io.netty.channel.Channel;
import io.netty.channel.ChannelId;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/** Owns CHILD registrations and the process-local MVP Relay sessions. */
public final class RelayRegistry {

    private final ConcurrentMap<String, Channel> childChannels = new ConcurrentHashMap<>();
    private final ConcurrentMap<ChannelId, String> agentSessionIds = new ConcurrentHashMap<>();
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
        private final String agentSessionId;
        private SessionState state;

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

        public Channel getAdminChannel() {
            return adminChannel;
        }
    }

    /**
     * Registers a CHILD channel. Repeating the same agentSessionId on the same channel
     * is accepted idempotently; all other duplicate agentSessionId/channel bindings
     * are rejected without replacing the existing registration.
     */
    public boolean registerChild(String agentSessionId, Channel channel) {
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
    public void unregisterChild(Channel channel) {
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
        if (isRegisteredChild(adminChannel)
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
            Channel peer = session.adminChannel == channel ? session.childChannel : session.adminChannel;
            peer.close(); // Both clients reset their transport/session state.
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
