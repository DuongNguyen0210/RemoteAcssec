package com.remotecontrol.relay.registry;

import io.netty.channel.Channel;
import io.netty.channel.ChannelId;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/** Owns the Phase 2 CHILD username-to-channel registrations. */
public final class RelayRegistry {

    private final ConcurrentMap<String, Channel> childChannels = new ConcurrentHashMap<>();
    private final ConcurrentMap<ChannelId, String> childUsernames = new ConcurrentHashMap<>();

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
}
