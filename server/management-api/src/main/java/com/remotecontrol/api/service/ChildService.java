package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.*;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
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
        if(!user.isPresent())
            return ApiResponse.error("Not found user");

        String fullChildUsername = currentUser.getUsername() + childUsername;
        Optional<Child> child = childRepository.findByUsername(fullChildUsername);
        if (child.isPresent())
            return ApiResponse.error("Child already exists");

        addChild(childUsername, password, user.get());
        ChildDto dto = ChildDto.builder()
                .username(fullChildUsername)
                .password(password)
                .online(false)
                .build();
        return ApiResponse.success("Accepted", dto);
    }

    public ApiResponse<List<ChildDto>> getListChildren(UserPrincipal currentUser)
    {
        if(!currentUser.getRole().equals("ADMIN"))
            return ApiResponse.error("Can not to access");

        Optional<User> user = userRepository.findByUsername(currentUser.getUsername());
        if(!user.isPresent())
            return ApiResponse.error("Not found user");

        List<Child> child = childRepository.findByOwner(user.get());
        List<ChildDto> childList = new ArrayList<>();
        for (Child c : child) {
            boolean isOnline = presenceService.isDeviceOnline(currentUser.getUsername(), c.getUsername());
            childList.add(new ChildDto(c.getUsername(), c.getPassword(), isOnline));
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

    public boolean handleHeartbeat(UserPrincipal currentUser, InfoPrincipal currentInfo) {
        if (!"CHILD".equals(currentUser.getRole()))
            return false;

        Optional<Child> c = childRepository.findByUsername(currentUser.getUsername());
        if (!c.isPresent()) {
            return false;
        }

        Child child = c.get();
        User parent = child.getOwner();
        presenceService.markDeviceOnline(parent, currentUser, currentInfo);
        return true;
    }

    public ApiResponse<Void> deleteChild(String childUsername, UserPrincipal currentUser) {
        if (!"ADMIN".equals(currentUser.getRole())) {
            return ApiResponse.error("Only ADMIN can delete devices");
        }

        Optional<Child> childOpt = childRepository.findByUsername(childUsername);
        if (!childOpt.isPresent()) {
            return ApiResponse.error("Device not found");
        }

        Child child = childOpt.get();
        if (!child.getOwner().getUsername().equals(currentUser.getUsername())) {
            return ApiResponse.error("Unauthorized to delete this device");
        }

        childRepository.delete(child);
        return ApiResponse.success("Deleted successfully");
    }
}

