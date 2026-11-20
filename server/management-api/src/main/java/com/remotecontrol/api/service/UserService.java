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
        if (userOpt.isEmpty()) {
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
        if (userOpt.isEmpty()) {
            return ApiResponse.error("USER_NOT_FOUND", "User not found");
        }
        User user = userOpt.get();
        if (!com.remotecontrol.api.util.ChildPasswords.matches(request.getOldPassword(), user.getPassword())) {
            return ApiResponse.error("INVALID_OLD_PASSWORD", "Old password does not match");
        }
        if (userRepository.upgradePassword(user.getId(), user.getPassword(),
                com.remotecontrol.api.util.ChildPasswords.encode(request.getNewPassword())) != 1)
            return ApiResponse.error("PASSWORD_CHANGED", "Credentials changed; please sign in again");
        return ApiResponse.success("Password changed successfully");
    }
}
