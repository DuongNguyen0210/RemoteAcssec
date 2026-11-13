package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.DeviceDto;
import org.junit.jupiter.api.Test;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

class DevicePresencePublisherTest {
    @Test void publishesExpiryWithoutAnotherHeartbeatAndSkipsOwnersWithoutSubscribers() throws Exception {
        var devices = mock(DeviceService.class);
        var notifier = new SseNotifier(new ObjectMapper());
        var emitter = mock(SseEmitter.class);
        var sent = new CountDownLatch(2);
        var payloads = new CopyOnWriteArrayList<String>();
        doAnswer(call -> {
            SseEmitter.SseEventBuilder event = call.getArgument(0);
            payloads.add(event.build().stream().map(part -> part.getData().toString())
                    .filter(part -> part.startsWith("[")).findFirst().orElse(""));
            sent.countDown();
            return null;
        }).when(emitter).send(any(SseEmitter.SseEventBuilder.class));
        when(devices.getDevices(any())).thenReturn(List.of(DeviceDto.builder().sessionId("a").build()), List.of());
        try (var publisher = new DevicePresencePublisher(devices, notifier)) {
            publisher.publish();
            verifyNoInteractions(devices);
            notifier.register(1L, emitter);
            publisher.publish();
            verify(emitter, timeout(2000)).send(any(SseEmitter.SseEventBuilder.class));
            // Wait for the owner task to release its slot, then reconcile again.
            long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(2);
            while (sent.getCount() > 0 && System.nanoTime() < deadline) {
                publisher.publish();
                Thread.sleep(10);
            }
            assertTrue(sent.await(1, TimeUnit.SECONDS));
            assertTrue(payloads.get(0).contains("\"sessionId\":\"a\""));
            assertEquals("[]", payloads.get(1));
        }
    }

    @Test void limitsConnectionsAndRemovesFailedStreams() throws Exception {
        var notifier = new SseNotifier(new ObjectMapper());
        for (int i = 0; i < 4; i++) notifier.register(1L, mock(SseEmitter.class));
        assertThrows(org.springframework.web.server.ResponseStatusException.class,
                () -> notifier.register(1L, mock(SseEmitter.class)));
        var broken = mock(SseEmitter.class);
        notifier.register(2L, broken);
        doThrow(new java.io.IOException("disconnected")).when(broken).send(any(SseEmitter.SseEventBuilder.class));
        notifier.notify(2L, List.of());
        assertEquals(0, notifier.connectionCount(2L));
        assertEquals(4, notifier.connectionCount(1L));
    }
}
