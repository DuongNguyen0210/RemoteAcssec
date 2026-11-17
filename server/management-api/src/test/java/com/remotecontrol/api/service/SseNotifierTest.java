package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.dto.common.UserPrincipal;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.*;

class SseNotifierTest {
    @Test
    void reconcilesOncePerOwnerAndPublishesExpiryWithoutAnotherHeartbeat() throws Exception {
        var devices = mock(DeviceService.class);
        var notifier = new SseNotifier(devices, new ObjectMapper());
        var owner = UserPrincipal.builder().id("10").build();
        var otherOwner = UserPrincipal.builder().id("11").build();
        var first = mock(SseEmitter.class);
        var second = mock(SseEmitter.class);
        var other = mock(SseEmitter.class);
        var initial = DeviceDto.builder().sessionId("agent").deviceName("Laptop").lastHeartbeatAt(1).build();
        var renewed = DeviceDto.builder().sessionId("agent").deviceName("Laptop").lastHeartbeatAt(2).build();
        notifier.register(10L, first, owner, List.of(initial));
        notifier.register(10L, second, owner, List.of(initial));
        notifier.register(11L, other, otherOwner, List.of());
        when(devices.getDevices(owner)).thenReturn(List.of(renewed)).thenReturn(List.of());
        when(devices.getDevices(otherOwner)).thenReturn(List.of());

        notifier.reconcile();

        verify(devices).getDevices(owner);
        verify(first, never()).send(any(SseEmitter.SseEventBuilder.class));
        verify(second, never()).send(any(SseEmitter.SseEventBuilder.class));

        notifier.reconcile();
        notifier.reconcile();

        var event = ArgumentCaptor.forClass(SseEmitter.SseEventBuilder.class);
        verify(first).send(event.capture());
        assertTrue(event.getValue().build().stream().anyMatch(part -> "[]".equals(part.getData())));
        verify(second).send(any(SseEmitter.SseEventBuilder.class));
        verify(other, never()).send(any(SseEmitter.SseEventBuilder.class));
    }
}
