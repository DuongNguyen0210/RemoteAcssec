package com.remotecontrol.relay;

import com.remotecontrol.relay.handler.RelayServerHandler;
import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.channel.DefaultChannelId;
import io.netty.channel.embedded.EmbeddedChannel;
import org.junit.jupiter.api.Test;

import java.time.Duration;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

import static org.junit.jupiter.api.Assertions.*;

class RelayPendingTimeoutTest {
    private static final Duration TIMEOUT = Duration.ofSeconds(1);

    private static RelayRegistry registry(AtomicLong now, AtomicInteger expirations) {
        return new RelayRegistry(TIMEOUT, now::get, session -> {
            expirations.incrementAndGet();
            RelayServerHandler.notifyPendingTimeout(session);
        });
    }

    private static EmbeddedChannel channel() {
        return new EmbeddedChannel(DefaultChannelId.newInstance());
    }

    private static void advanceToDeadline(EmbeddedChannel admin, AtomicLong now) {
        now.addAndGet(TIMEOUT.toNanos());
        admin.advanceTimeBy(TIMEOUT.toNanos(), TimeUnit.NANOSECONDS);
        admin.runScheduledPendingTasks();
    }

    @Test void timeoutClearsAllMappingsAndSendsFailure() {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var child = channel();
        try {
            assertTrue(registry.registerChild("child", child));
            var session = registry.createPendingSession(admin, child, "child");
            assertNotNull(session);
            assertEquals(1, registry.pendingSessionCount());
            assertTrue(registry.hasSession(admin));
            assertTrue(registry.hasSession(child));

            advanceToDeadline(admin, now);

            assertEquals(1, expirations.get());
            assertEquals(0, registry.pendingSessionCount());
            assertFalse(registry.hasSession(admin));
            assertFalse(registry.hasSession(child));
            assertNull(registry.findRegisteredChild("child"));
            assertFalse(child.isActive());
            Protocol failure = admin.readOutbound();
            assertNotNull(failure);
            assertEquals(ProtocolConstants.MessageType.CONNECT_RESULT.getValue(), failure.getHeader().getType());
            assertEquals(0, failure.getHeader().getSessionId());
            assertArrayEquals(new byte[]{0}, failure.getPayload());
            assertNull(admin.readOutbound());
        } finally {
            child.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }

    @Test void acceptBeforeDeadlineCancelsTimeout() {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var child = channel();
        try {
            assertTrue(registry.registerChild("child", child));
            var session = registry.createPendingSession(admin, child, "child");
            assertSame(session, registry.acceptPendingSession(session.getSessionId(), child));

            advanceToDeadline(admin, now);

            assertEquals(0, expirations.get());
            assertEquals(1, registry.activeSessionCount());
            assertTrue(child.isActive());
            assertNull(admin.readOutbound());
        } finally {
            child.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }

    @Test void timeoutWinsAndLateAcceptOrRejectIsIgnored() {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var child = channel();
        try {
            assertTrue(registry.registerChild("child", child));
            var session = registry.createPendingSession(admin, child, "child");
            advanceToDeadline(admin, now);

            assertNull(registry.acceptPendingSession(session.getSessionId(), child));
            assertNull(registry.rejectPendingSession(session.getSessionId(), child));
            assertEquals(1, expirations.get());
            assertEquals(0, registry.pendingSessionCount());
        } finally {
            child.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }

    @Test void acceptAtDeadlineCannotTurnPendingActiveBeforeTimerRuns() {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var child = channel();
        try {
            assertTrue(registry.registerChild("child", child));
            var session = registry.createPendingSession(admin, child, "child");
            now.addAndGet(TIMEOUT.toNanos());

            assertNull(registry.acceptPendingSession(session.getSessionId(), child));
            assertEquals(0, registry.activeSessionCount());
            admin.advanceTimeBy(TIMEOUT.toNanos(), TimeUnit.NANOSECONDS);
            admin.runScheduledPendingTasks();
            assertEquals(1, expirations.get());
            assertEquals(0, registry.pendingSessionCount());
        } finally {
            child.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }

    @Test void rejectCancelsTimeoutAndFreesBothChannels() {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var child = channel();
        try {
            assertTrue(registry.registerChild("child", child));
            var session = registry.createPendingSession(admin, child, "child");
            assertSame(session, registry.rejectPendingSession(session.getSessionId(), child));

            advanceToDeadline(admin, now);

            assertEquals(0, expirations.get());
            assertEquals(0, registry.pendingSessionCount());
            assertFalse(registry.hasSession(admin));
            assertFalse(registry.hasSession(child));
            assertTrue(child.isActive());
        } finally {
            child.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }

    @Test void disconnectCancelsTimeout() {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var child = channel();
        try {
            assertTrue(registry.registerChild("child", child));
            var session = registry.createPendingSession(admin, child, "child");
            assertNotNull(session);
            registry.removeSessionForChannel(child);

            advanceToDeadline(admin, now);

            assertEquals(0, expirations.get());
            assertEquals(0, registry.pendingSessionCount());
            assertFalse(registry.hasSession(admin));
            assertFalse(registry.hasSession(child));
            assertFalse(admin.isActive());
        } finally {
            child.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }

    @Test void adminDisconnectCancelsTimeout() {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var child = channel();
        try {
            assertTrue(registry.registerChild("child", child));
            assertNotNull(registry.createPendingSession(admin, child, "child"));
            registry.removeSessionForChannel(admin);

            advanceToDeadline(admin, now);

            assertEquals(0, expirations.get());
            assertEquals(0, registry.pendingSessionCount());
            assertFalse(registry.hasSession(admin));
            assertFalse(registry.hasSession(child));
            assertFalse(child.isActive());
        } finally {
            child.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }

    @Test void sameAdminCanRetryAfterChildRegistersAgain() {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var firstChild = channel();
        var reconnectedChild = channel();
        try {
            assertTrue(registry.registerChild("child", firstChild));
            var first = registry.createPendingSession(admin, firstChild, "child");
            assertNotNull(first);
            advanceToDeadline(admin, now);
            assertEquals(1, expirations.get());
            assertTrue(admin.isActive());
            assertFalse(firstChild.isActive());

            assertTrue(registry.registerChild("child", reconnectedChild));
            var retry = registry.createPendingSession(admin, reconnectedChild, "child");
            assertNotNull(retry);
            assertNotEquals(first.getSessionId(), retry.getSessionId());
            assertSame(retry, registry.acceptPendingSession(retry.getSessionId(), reconnectedChild));
            assertEquals(1, registry.activeSessionCount());
        } finally {
            reconnectedChild.finishAndReleaseAll();
            firstChild.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }
    @Test void acceptAfterDeadlineExpiresImmediatelyEvenWhenTimerIsDelayed() {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var child = channel();
        try {
            assertTrue(registry.registerChild("child", child));
            var session = registry.createPendingSession(admin, child, "child");
            now.addAndGet(TIMEOUT.toNanos() + 1);
            assertNull(registry.acceptPendingSession(session.getSessionId(), child));
            assertEquals(1, expirations.get());
            assertEquals(0, registry.pendingSessionCount());
            assertEquals(0, registry.activeSessionCount());
            assertFalse(registry.hasSession(admin));
            assertFalse(registry.hasSession(child));
            advanceToDeadline(admin, now);
            assertEquals(1, expirations.get());
        } finally {
            child.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }
    @Test void rejectAtDeadlineExpiresBeforeDelayedTimer() {
        assertRejectExpiresBeforeTimer(0);
    }

    @Test void rejectAfterDeadlineExpiresBeforeDelayedTimer() {
        assertRejectExpiresBeforeTimer(1);
    }

    private void assertRejectExpiresBeforeTimer(long extraNanos) {
        var now = new AtomicLong();
        var expirations = new AtomicInteger();
        var registry = registry(now, expirations);
        var admin = channel();
        var child = channel();
        try {
            assertTrue(registry.registerChild("child", child));
            var session = registry.createPendingSession(admin, child, "child");
            now.set(TIMEOUT.toNanos() + extraNanos);
            assertNull(registry.rejectPendingSession(session.getSessionId(), child));
            assertEquals(1, expirations.get());
            assertEquals(0, registry.pendingSessionCount());
            assertEquals(0, registry.activeSessionCount());
            assertFalse(registry.hasSession(admin));
            assertFalse(registry.hasSession(child));
            assertNull(registry.findRegisteredChild("child"));
            assertFalse(registry.isRegisteredChild(child));
            assertFalse(child.isActive());
            assertTrue(admin.isActive());
            Protocol failure = admin.readOutbound();
            assertNotNull(failure);
            assertEquals(ProtocolConstants.MessageType.CONNECT_RESULT.getValue(), failure.getHeader().getType());
            assertEquals(0, failure.getHeader().getSessionId());
            assertArrayEquals(new byte[]{0}, failure.getPayload());
            assertNull(admin.readOutbound());
            assertNull(registry.acceptPendingSession(session.getSessionId(), child));
            assertNull(registry.rejectPendingSession(session.getSessionId(), child));
            admin.advanceTimeBy(TIMEOUT.toNanos(), TimeUnit.NANOSECONDS);
            admin.runScheduledPendingTasks();
            assertEquals(1, expirations.get());
            assertNull(admin.readOutbound());
        } finally {
            child.finishAndReleaseAll();
            admin.finishAndReleaseAll();
        }
    }

    @Test void timeoutRetiresOldChannelBeforeCallbackAndPreservesReconnectedSession() {
        var now = new AtomicLong();
        var admin = channel();
        var oldChild = channel();
        var newAdmin = channel();
        var newChild = channel();
        var expirations = new AtomicInteger();
        RelayRegistry[] holder = new RelayRegistry[1];
        holder[0] = new RelayRegistry(TIMEOUT, now::get, expired -> {
            expirations.incrementAndGet();
            var registry = holder[0];
            assertFalse(registry.registerChild("child", oldChild));
            assertTrue(registry.registerChild("child", newChild));
            var retry = registry.createPendingSession(newAdmin, newChild, "child");
            assertNotNull(retry);
            assertSame(retry, registry.acceptPendingSession(retry.getSessionId(), newChild));
            registry.removeSessionForChannel(oldChild);
            registry.unregisterChild(oldChild);
            RelayServerHandler.notifyPendingTimeout(expired);
        });
        var registry = holder[0];
        try {
            assertTrue(registry.registerChild("child", oldChild));
            var session = registry.createPendingSession(admin, oldChild, "child");
            now.set(TIMEOUT.toNanos());
            assertNull(registry.rejectPendingSession(session.getSessionId(), oldChild));
            assertEquals(1, expirations.get());
            assertFalse(oldChild.isActive());
            assertTrue(newChild.isActive());
            assertTrue(newAdmin.isActive());
            assertSame(newChild, registry.findRegisteredChild("child"));
            assertEquals(1, registry.activeSessionCount());
        } finally {
            oldChild.finishAndReleaseAll();
            newChild.finishAndReleaseAll();
            admin.finishAndReleaseAll();
            newAdmin.finishAndReleaseAll();
        }
    }
}
