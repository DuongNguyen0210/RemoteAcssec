package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.DeviceDto;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;
import java.util.List;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.*;

class SseNotifierTest {
    @Test void reconcilesOncePerOwnerAndKeepsOwnerSnapshotsIsolated() throws Exception {
        var devices = mock(DeviceService.class);
        var notifier = new SseNotifier(new ObjectMapper());
        var first = mock(SseEmitter.class);
        var second = mock(SseEmitter.class);
        var other = mock(SseEmitter.class);
        notifier.register(10L, first);
        notifier.register(10L, second);
        notifier.register(11L, other);
        when(devices.getDevices(any())).thenAnswer(call -> {
            com.remotecontrol.api.dto.common.UserPrincipal owner = call.getArgument(0);
            return owner.getId().equals("10")
                    ? List.of(DeviceDto.builder().sessionId("owner-10-agent").build()) : List.of();
        });
        try (var publisher = new DevicePresencePublisher(devices, notifier)) {
            publisher.publish();
            verify(first, timeout(2000)).send(any(SseEmitter.SseEventBuilder.class));
            verify(second, timeout(2000)).send(any(SseEmitter.SseEventBuilder.class));
            verify(other, timeout(2000)).send(any(SseEmitter.SseEventBuilder.class));
            verify(devices, times(2)).getDevices(any());
            var event = ArgumentCaptor.forClass(SseEmitter.SseEventBuilder.class);
            verify(first).send(event.capture());
            assertTrue(event.getValue().build().stream().anyMatch(part -> part.getData().toString().contains("owner-10-agent")));
            verify(other).send(event.capture());
            assertTrue(event.getValue().build().stream().anyMatch(part -> "[]".equals(part.getData())));
        }
    }
}
