package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.web.server.ResponseStatusException;

import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

@Service
@RequiredArgsConstructor
@Slf4j
public class DeviceService {

    private final PresenceService presenceService;
    private final ChildRepository childRepository;
    private final UserRepository userRepository;

    public List<DeviceDto> getDevices(UserPrincipal principal) {

        User owner = userRepository.findById(Long.valueOf(principal.getId()))
                .orElseThrow(() -> new ResponseStatusException(HttpStatus.UNAUTHORIZED));
        Set<Long> ownedIds = childRepository.findByOwner(owner).stream()
                .map(Child::getId)
                .collect(Collectors.toSet());
        return presenceService.getDevices(owner.getId()).stream()
                .filter(d -> ownedIds.contains(d.getChildId()))
                .toList();
    }
}
