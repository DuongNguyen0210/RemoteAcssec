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

        Child created = addChild(childUsername, password, user.get());
        ChildDto dto = ChildDto.builder()
                .id(created.getId())
                .username(fullChildUsername)
                .childUsername(childUsername)
                .build();
        return ApiResponse.success("Accepted", dto);
    }

    public ApiResponse<List<ChildDto>> getListChildren(UserPrincipal currentUser) {
        Optional<User> user = userRepository.findByUsername(currentUser.getUsername());
        if (user.isEmpty()) {
            return ApiResponse.error("USER_NOT_FOUND", "User not found");
        }

        List<Child> children = childRepository.findByOwner(user.get());
        List<ChildDto> childList = new ArrayList<>();
        for (Child c : children) {
            String pureChildUsername = c.getUsername();
            if (pureChildUsername.startsWith(currentUser.getUsername())) {
                pureChildUsername = pureChildUsername.substring(currentUser.getUsername().length());
            }

            childList.add(ChildDto.builder()
                    .id(c.getId())
                    .username(c.getUsername())
                    .childUsername(pureChildUsername)
                    .build());
        }

        return ApiResponse.success("Accepted", childList);
    }

    public Child addChild(String childUsername, String password, User user) {
        Child newChild = Child.builder()
                .username(user.getUsername() + childUsername)
                .password(password)
                .owner(user)
                .build();
        return childRepository.save(newChild);
    }

    public boolean handleHeartbeat(UserPrincipal currentUser, InfoPrincipal currentInfo, HeartbeatRequest request) {
        Optional<Child> c = childRepository.findById(Long.valueOf(currentUser.getId()));
        if (c.isEmpty()) {
            return false;
        }

        Child child = c.get();
        User parent = child.getOwner();
        return presenceService.markDeviceOnline(parent, currentUser, currentInfo, request);
    }

    public void logout(UserPrincipal principal) {
        childRepository.findById(Long.valueOf(principal.getId())).ifPresent(child ->
                presenceService.closeSession(child.getOwner().getId(), principal.getSessionId()));
    }

    @org.springframework.transaction.annotation.Transactional
    public ApiResponse<ChildDto> updateChild(Long id,
            com.remotecontrol.api.dto.child.UpdateChildRequest request, UserPrincipal principal) {
        var found = childRepository.findById(id);
        if (found.isEmpty()) return ApiResponse.error("CHILD_NOT_FOUND", "Account not found");
        Child child = found.get();
        if (!"ADMIN".equals(principal.getRole())
                || !String.valueOf(child.getOwner().getId()).equals(principal.getId())) {
            return ApiResponse.error("FORBIDDEN", "Unauthorized to edit this account");
        }
        String suffix = request.getChildUsername() == null ? "" : request.getChildUsername().trim();
        String username = child.getOwner().getUsername() + suffix;
        String password = request.getNewPassword();
        if (suffix.isBlank() || username.length() > 50
                || (password != null && !password.isEmpty() && (password.isBlank() || password.length() > 255))) {
            return ApiResponse.error("INVALID_INPUT", "Invalid account name or password");
        }
        var duplicate = childRepository.findByUsername(username);
        if (duplicate.isPresent() && !duplicate.get().getId().equals(id)) {
            return ApiResponse.error("CHILD_ALREADY_EXISTS", "Account name already exists");
        }
        child.setUsername(username);
        if (password != null && !password.isEmpty()) {
            child.setPassword(com.remotecontrol.api.util.ChildPasswords.encode(password));
        }
        childRepository.saveAndFlush(child);
        return ApiResponse.success("Account updated", ChildDto.builder()
                .id(id).username(username).childUsername(suffix).build());
    }

    public ApiResponse<Void> deleteChild(String childUsername, UserPrincipal currentUser) {
        Optional<Child> childOpt = childRepository.findByUsername(childUsername);
        if (childOpt.isEmpty()) {
            childOpt = childRepository.findByUsername(currentUser.getUsername() + childUsername);
        }
        if (childOpt.isEmpty()) {
            return ApiResponse.error("CHILD_NOT_FOUND", "Account not found");
        }

        Child child = childOpt.get();
        if (!child.getOwner().getUsername().equals(currentUser.getUsername())) {
            return ApiResponse.error("FORBIDDEN", "Unauthorized to delete this account");
        }

        childRepository.delete(child);
        return ApiResponse.success("Deleted successfully");
    }
}
