package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.common.ApiResponse;
import com.remotecontrol.api.dto.common.InfoPrincipal;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.dto.child.ChildDto;
import com.remotecontrol.api.dto.child.HeartbeatRequest;
import com.remotecontrol.api.dto.child.RegisterRequest;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;

@Service
@RequiredArgsConstructor
@Slf4j
public class ChildService {

    private final ChildRepository childRepository;
    private final UserRepository userRepository;
    private final PresenceService presenceService;

    public ApiResponse<ChildDto> register(RegisterRequest request, UserPrincipal currentUser) {
        String childUsername = request.getChildUsername();
        String password = request.getPassword();

        Optional<User> user = userRepository.findByUsername(currentUser.getUsername());
        if (user.isEmpty()) {
            return ApiResponse.error("USER_NOT_FOUND", "User not found");
        }

        List<Child> existingChildren = childRepository.findByOwner(user.get());
        if (user.get().getMaxChildren() != null && existingChildren.size() >= user.get().getMaxChildren()) {
            return ApiResponse.error("LIMIT_EXCEEDED", "Maximum child accounts limit reached");
        }

        String fullChildUsername = currentUser.getUsername() + childUsername;
        Optional<Child> child = childRepository.findByUsername(fullChildUsername);
        if (child.isPresent()) {
            return ApiResponse.error("CHILD_ALREADY_EXISTS", "Child already exists");
        }

        addChild(childUsername, password, user.get());
        ChildDto dto = ChildDto.builder()
                .username(fullChildUsername)
                .childUsername(childUsername)
                .password(password)
                .online(false)
                .build();
        return ApiResponse.success("Accepted", dto);
    }

    public ApiResponse<List<ChildDto>> getListChildren(UserPrincipal currentUser) {
        if (!"ADMIN".equals(currentUser.getRole())) {
            return ApiResponse.error("FORBIDDEN", "Only admin can access children list");
        }

        Optional<User> user = userRepository.findByUsername(currentUser.getUsername());
        if (user.isEmpty()) {
            return ApiResponse.error("USER_NOT_FOUND", "User not found");
        }

        List<Child> children = childRepository.findByOwner(user.get());
        List<ChildDto> childList = new ArrayList<>();
        for (Child c : children) {
            boolean isOnline = presenceService.isDeviceOnline(currentUser.getUsername(), c.getUsername());
            Map<Object, Object> devInfo = presenceService.getDeviceInfo(c.getUsername());
            String ip = (devInfo != null && devInfo.get("ip") != null) ? devInfo.get("ip").toString() : null;
            String os = (devInfo != null && devInfo.get("os") != null) ? devInfo.get("os").toString() : null;
            String deviceUid = (devInfo != null && devInfo.get("deviceUid") != null) ? devInfo.get("deviceUid").toString() : null;
            String deviceName = (devInfo != null && devInfo.get("deviceName") != null) ? devInfo.get("deviceName").toString() : null;

            String pureChildUsername = c.getUsername();
            if (pureChildUsername.startsWith(currentUser.getUsername())) {
                pureChildUsername = pureChildUsername.substring(currentUser.getUsername().length());
            }

            childList.add(ChildDto.builder()
                    .username(c.getUsername())
                    .childUsername(pureChildUsername)
                    .password(c.getPassword())
                    .online(isOnline)
                    .ipAddress(ip)
                    .os(os)
                    .deviceUid(deviceUid)
                    .deviceName(deviceName)
                    .build());
        }

        return ApiResponse.success("Accepted", childList);
    }

    public void addChild(String childUsername, String password, User user) {
        Child newChild = Child.builder()
                .username(user.getUsername() + childUsername)
                .password(password)
                .owner(user)
                .build();
        childRepository.save(newChild);
    }

    public boolean handleHeartbeat(UserPrincipal currentUser, InfoPrincipal currentInfo, HeartbeatRequest request) {
        if (!"CHILD".equals(currentUser.getRole())) {
            return false;
        }

        Optional<Child> c = childRepository.findByUsername(currentUser.getUsername());
        if (c.isEmpty()) {
            return false;
        }

        Child child = c.get();
        User parent = child.getOwner();
        presenceService.markDeviceOnline(parent, currentUser, currentInfo, request);
        return true;
    }

    public boolean handleHeartbeat(UserPrincipal currentUser, InfoPrincipal currentInfo) {
        return handleHeartbeat(currentUser, currentInfo, null);
    }

    public ApiResponse<Void> deleteChild(String childUsername, UserPrincipal currentUser) {
        if (!"ADMIN".equals(currentUser.getRole())) {
            return ApiResponse.error("FORBIDDEN", "Only ADMIN can delete devices");
        }

        Optional<Child> childOpt = childRepository.findByUsername(childUsername);
        if (childOpt.isEmpty()) {
            childOpt = childRepository.findByUsername(currentUser.getUsername() + childUsername);
        }
        if (childOpt.isEmpty()) {
            return ApiResponse.error("DEVICE_NOT_FOUND", "Device not found");
        }

        Child child = childOpt.get();
        if (!child.getOwner().getUsername().equals(currentUser.getUsername())) {
            return ApiResponse.error("FORBIDDEN", "Unauthorized to delete this device");
        }

        childRepository.delete(child);
        return ApiResponse.success("Deleted successfully");
    }
}
