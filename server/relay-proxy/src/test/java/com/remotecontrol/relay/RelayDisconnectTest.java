package com.remotecontrol.relay;

import com.remotecontrol.relay.handler.RelayServerHandler;
import com.remotecontrol.relay.protocol.*;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.channel.DefaultChannelId;
import io.netty.channel.embedded.EmbeddedChannel;
import org.junit.jupiter.api.Test;
import com.remotecontrol.relay.auth.RelayAuthorizer;
import com.remotecontrol.relay.auth.RelayAuthPayload;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFuture;
import io.netty.channel.DefaultChannelPromise;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Function;
import static org.junit.jupiter.api.Assertions.*;

class RelayDisconnectTest {
    private Protocol disconnect(long id, short flags, byte[] payload) {
        return new Protocol(new ProtocolHeader(ProtocolConstants.PROTOCOL_MAGIC,
                ProtocolConstants.PROTOCOL_VERSION, ProtocolConstants.MessageType.DISCONNECT.getValue(),
                flags, payload.length, id, 0), payload);
    }
    private EmbeddedChannel channel(RelayRegistry registry) {
        return new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
    }
    private void notifyChild(RelayServerHandler handler, RelayRegistry.SessionRecord session,
                             Function<Channel, ChannelFuture> writer) throws Exception {
        var method = RelayServerHandler.class.getDeclaredMethod("notifyDisconnectedChild",
                RelayRegistry.SessionRecord.class, Function.class);
        method.setAccessible(true);
        method.invoke(handler, session, writer);
    }
    @Test void disconnectChecksOwnershipAndNotifiesOnceAndAllowsNewSession() {
        var registry = new RelayRegistry(session -> {});
        var admin = channel(registry); var child = channel(registry); var stranger = channel(registry);
        var otherAdmin = channel(registry); var otherChild = channel(registry);
        try {
            assertTrue(registry.registerChild("one", child));
            assertTrue(registry.registerChild("two", otherChild));
            var first = registry.createPendingSession(admin, child, "one");
            var other = registry.createPendingSession(otherAdmin, otherChild, "two");
            assertNotNull(registry.acceptPendingSession(first.getSessionId(), child));
            assertNotNull(registry.acceptPendingSession(other.getSessionId(), otherChild));
            long id = first.getSessionId();
            stranger.writeInbound(disconnect(id, (short)0, new byte[0]));
            child.writeInbound(disconnect(id, (short)0, new byte[0]));
            admin.writeInbound(disconnect(id+100, (short)0, new byte[0]));
            admin.writeInbound(disconnect(id, (short)1, new byte[0]));
            admin.writeInbound(disconnect(id, (short)0, new byte[]{1}));
            assertNull(child.readOutbound()); assertEquals(2, registry.activeSessionCount());
            admin.writeInbound(disconnect(id, (short)0, new byte[0]));
            var notification = (Protocol) child.readOutbound();
            assertNotNull(notification); assertEquals(id, notification.getHeader().getSessionId());
            assertEquals(ProtocolConstants.MessageType.DISCONNECT.getValue(), notification.getHeader().getType());
            assertFalse(registry.hasSession(admin)); assertFalse(registry.hasSession(child));
            assertEquals(1, registry.activeSessionCount()); assertTrue(child.isActive()); assertTrue(admin.isActive());
            admin.writeInbound(disconnect(id, (short)0, new byte[0]));
            assertNull(child.readOutbound());
            var retry = registry.createPendingSession(admin, child, "one");
            assertNotNull(retry); assertNotNull(registry.acceptPendingSession(retry.getSessionId(), child));
            admin.writeInbound(disconnect(id, (short)0, new byte[0]));
            assertEquals(2, registry.activeSessionCount()); assertNull(child.readOutbound());
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); stranger.finishAndReleaseAll(); otherAdmin.finishAndReleaseAll(); otherChild.finishAndReleaseAll(); }
    }
    @Test void pendingAndDisconnectAfterTransportCloseAreNoOps() {
        var registry = new RelayRegistry(session -> {});
        var admin = channel(registry); var child = channel(registry);
        try {
            assertTrue(registry.registerChild("one", child));
            var pending = registry.createPendingSession(admin, child, "one");
            admin.writeInbound(disconnect(pending.getSessionId(), (short)0, new byte[0]));
            assertNull(child.readOutbound()); assertEquals(1, registry.pendingSessionCount());
            registry.removeSessionForChannel(admin);
            assertNull(registry.beginDisconnect(pending.getSessionId(), admin));
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); }
    }

    @Test void staleLeaseResultCannotCloseAdminAfterDisconnect() throws Exception {
        var result = new CompletableFuture<String>();
        var authorizer = new RelayAuthorizer() {
            @Override public CompletableFuture<String> authorize(String token, String target) { return result; }
        };
        var registry = new RelayRegistry(session -> {});
        var handler = new RelayServerHandler(registry, authorizer);
        var admin = new EmbeddedChannel(DefaultChannelId.newInstance(), handler);
        var child = channel(registry);
        try {
            assertTrue(registry.registerChild("one", child));
            var session = registry.createPendingSession(admin, child, "one");
            assertNotNull(registry.acceptPendingSession(session.getSessionId(), child));
            var method = RelayServerHandler.class.getDeclaredMethod("startLease", ChannelHandlerContext.class,
                    RelayAuthPayload.class, String.class);
            method.setAccessible(true);
            method.invoke(handler, admin.pipeline().context(handler), new RelayAuthPayload("token", "target"), "one");
            admin.advanceTimeBy(10, TimeUnit.SECONDS);
            admin.runScheduledPendingTasks();
            admin.writeInbound(disconnect(session.getSessionId(), (short)0, new byte[0]));
            assertNotNull(child.readOutbound());
            result.complete(null);
            admin.runPendingTasks();
            assertTrue(admin.isActive());
            assertNotNull(registry.createPendingSession(admin, child, "one"));
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); }
    }

    @Test void gracefulAndTransportDisconnectRaceCleansAtMostOnce() throws Exception {
        for (int i = 0; i < 20; i++) {
            var registry = new RelayRegistry(session -> {});
            var admin = new EmbeddedChannel(DefaultChannelId.newInstance());
            var child = new EmbeddedChannel(DefaultChannelId.newInstance());
            var executor = Executors.newFixedThreadPool(2);
            try {
                assertTrue(registry.registerChild("one", child));
                var session = registry.createPendingSession(admin, child, "one");
                assertNotNull(registry.acceptPendingSession(session.getSessionId(), child));
                var barrier = new CyclicBarrier(2);
                var notifications = new AtomicInteger();
                var graceful = executor.submit(() -> {
                    barrier.await();
                    var result = registry.beginDisconnect(session.getSessionId(), admin);
                    if (result != null) {
                        notifications.incrementAndGet();
                        registry.completeDisconnect(result, true);
                    }
                    return null;
                });
                var transport = executor.submit(() -> {
                    barrier.await();
                    registry.removeSessionForChannel(admin);
                    return null;
                });
                graceful.get(); transport.get();
                assertTrue(notifications.get() <= 1);
                assertEquals(0, registry.activeSessionCount());
                assertFalse(registry.hasSession(admin)); assertFalse(registry.hasSession(child));
            } finally {
                executor.shutdownNow(); admin.finishAndReleaseAll(); child.finishAndReleaseAll();
            }
        }
    }

    @Test void childCloseDuringNotificationKeepsAdminAndOtherSessionAlive() throws Exception {
        var registry = new RelayRegistry(session -> {});
        var handler = new RelayServerHandler(registry);
        var admin = channel(registry); var child = channel(registry);
        var otherAdmin = channel(registry); var otherChild = channel(registry);
        var replacement = channel(registry);
        try {
            assertTrue(registry.registerChild("one", child));
            assertTrue(registry.registerChild("two", otherChild));
            assertTrue(registry.registerChild("replacement", replacement));
            var session = registry.createPendingSession(admin, child, "one");
            var other = registry.createPendingSession(otherAdmin, otherChild, "two");
            assertNotNull(registry.acceptPendingSession(session.getSessionId(), child));
            assertNotNull(registry.acceptPendingSession(other.getSessionId(), otherChild));
            var ended = registry.beginDisconnect(session.getSessionId(), admin);
            assertSame(session, ended);
            var calls = new AtomicInteger();
            notifyChild(handler, ended, channel -> {
                assertFalse(Thread.holdsLock(registry));
                calls.incrementAndGet();
                channel.close(); // Triggers channelInactive before the failed write is handled.
                return channel.newFailedFuture(new IllegalStateException("closed"));
            });
            child.runPendingTasks();
            assertEquals(1, calls.get());
            assertNull(registry.beginDisconnect(session.getSessionId(), admin));
            assertTrue(admin.isActive()); assertFalse(child.isActive());
            assertNull(registry.findRegisteredChild("one"));
            assertNotNull(registry.findActivePeer(other.getSessionId(), otherAdmin));
            assertNotNull(registry.createPendingSession(admin, replacement, "replacement"));
        } finally {
            admin.finishAndReleaseAll(); child.finishAndReleaseAll(); otherAdmin.finishAndReleaseAll();
            otherChild.finishAndReleaseAll(); replacement.finishAndReleaseAll();
        }
    }

    @Test void thrownWriterCleansReservationAndClosesOnlyChild() throws Exception {
        var registry = new RelayRegistry(session -> {});
        var handler = new RelayServerHandler(registry);
        var admin = channel(registry); var child = channel(registry); var replacement = channel(registry);
        try {
            assertTrue(registry.registerChild("one", child));
            assertTrue(registry.registerChild("replacement", replacement));
            var session = registry.createPendingSession(admin, child, "one");
            assertNotNull(registry.acceptPendingSession(session.getSessionId(), child));
            var ended = registry.beginDisconnect(session.getSessionId(), admin);
            notifyChild(handler, ended, channel -> {
                assertFalse(Thread.holdsLock(registry));
                throw new IllegalStateException("enqueue failed");
            });
            child.runPendingTasks();
            assertTrue(admin.isActive()); assertFalse(child.isActive());
            assertNull(registry.findRegisteredChild("one"));
            assertFalse(registry.hasSession(admin)); assertFalse(registry.hasSession(child));
            assertNull(registry.beginDisconnect(session.getSessionId(), admin));
            assertFalse(registry.completeDisconnect(ended, false));
            assertNotNull(registry.createPendingSession(admin, replacement, "replacement"));
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); replacement.finishAndReleaseAll(); }
    }

    @Test void childIsReservedUntilNotificationFutureCompletes() throws Exception {
        var registry = new RelayRegistry(session -> {});
        var handler = new RelayServerHandler(registry);
        var admin = channel(registry); var child = channel(registry); var nextAdmin = channel(registry);
        try {
            assertTrue(registry.registerChild("one", child));
            var session = registry.createPendingSession(admin, child, "one");
            assertNotNull(registry.acceptPendingSession(session.getSessionId(), child));
            var ended = registry.beginDisconnect(session.getSessionId(), admin);
            var pending = new DefaultChannelPromise(child);
            var count = new AtomicInteger();
            notifyChild(handler, ended, channel -> {
                assertFalse(Thread.holdsLock(registry));
                count.incrementAndGet();
                return pending;
            });
            assertNull(registry.beginDisconnect(session.getSessionId(), admin));
            assertNull(registry.createPendingSession(nextAdmin, child, "one"));
            assertTrue(admin.isActive());
            pending.setSuccess(); child.runPendingTasks();
            assertEquals(1, count.get());
            assertNotNull(registry.createPendingSession(nextAdmin, child, "one"));
            assertTrue(admin.isActive());
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); nextAdmin.finishAndReleaseAll(); }
    }

    @Test void delayedWriteFailureAfterChildInactiveDoesNotCloseAdmin() throws Exception {
        var registry = new RelayRegistry(session -> {});
        var handler = new RelayServerHandler(registry);
        var admin = channel(registry); var child = channel(registry); var replacement = channel(registry);
        try {
            assertTrue(registry.registerChild("one", child));
            assertTrue(registry.registerChild("replacement", replacement));
            var session = registry.createPendingSession(admin, child, "one");
            assertNotNull(registry.acceptPendingSession(session.getSessionId(), child));
            var ended = registry.beginDisconnect(session.getSessionId(), admin);
            var pending = new DefaultChannelPromise(child);
            notifyChild(handler, ended, channel -> pending);
            child.close(); child.runPendingTasks();
            assertTrue(admin.isActive());
            pending.setFailure(new IllegalStateException("late failure")); child.runPendingTasks();
            assertTrue(admin.isActive());
            assertNull(registry.findRegisteredChild("one"));
            assertNotNull(registry.createPendingSession(admin, replacement, "replacement"));
            assertFalse(registry.completeDisconnect(ended, false));
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); replacement.finishAndReleaseAll(); }
    }

    @Test void stalledWriteTimesOutAndCannotLeaveReservationStuck() throws Exception {
        var registry = new RelayRegistry(session -> {});
        var handler = new RelayServerHandler(registry);
        var admin = channel(registry); var child = channel(registry); var replacement = channel(registry);
        try {
            assertTrue(registry.registerChild("one", child));
            assertTrue(registry.registerChild("replacement", replacement));
            var session = registry.createPendingSession(admin, child, "one");
            assertNotNull(registry.acceptPendingSession(session.getSessionId(), child));
            var ended = registry.beginDisconnect(session.getSessionId(), admin);
            var pending = new DefaultChannelPromise(child);
            notifyChild(handler, ended, channel -> pending);
            child.advanceTimeBy(15, TimeUnit.SECONDS);
            child.runScheduledPendingTasks(); child.runPendingTasks();
            assertTrue(admin.isActive()); assertFalse(child.isActive());
            assertNull(registry.findRegisteredChild("one"));
            assertNotNull(registry.createPendingSession(admin, replacement, "replacement"));
            pending.setFailure(new IllegalStateException("late")); child.runPendingTasks();
            assertTrue(admin.isActive());
            assertFalse(registry.completeDisconnect(ended, false));
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); replacement.finishAndReleaseAll(); }
    }
}
