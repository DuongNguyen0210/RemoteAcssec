package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.common.ApiResponse;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.dto.user.ChangePasswordRequest;
import com.remotecontrol.api.dto.user.UserDto;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;

import java.util.Objects;
import java.util.Optional;

@Service
@RequiredArgsConstructor
public class UserService {

    private final UserRepository userRepository;

    public ApiResponse<UserDto> getProfile(UserPrincipal currentUser) {
        Optional<User> userOpt = userRepository.findByUsername(currentUser.getUsername());
        if (!userOpt.isPresent()) {
            return ApiResponse.error("USER_NOT_FOUND", "User not found");
        }
        User user = userOpt.get();
        UserDto dto = UserDto.builder()
                .id(user.getId())
                .username(user.getUsername())
                .role(currentUser.getRole())
                .maxChildren(user.getMaxChildren())
                .build();
        return ApiResponse.success("User profile retrieved successfully", dto);
    }

    public ApiResponse<Void> changePassword(ChangePasswordRequest request, UserPrincipal currentUser) {
        Optional<User> userOpt = userRepository.findByUsername(currentUser.getUsername());
        if (!userOpt.isPresent()) {
            return ApiResponse.error("USER_NOT_FOUND", "User not found");
        }
        User user = userOpt.get();
        if (!Objects.equals(user.getPassword(), request.getOldPassword())) {
            return ApiResponse.error("INVALID_OLD_PASSWORD", "Old password does not match");
        }
        user.setPassword(request.getNewPassword());
        userRepository.save(user);
        return ApiResponse.success("Password changed successfully");
    }
}
