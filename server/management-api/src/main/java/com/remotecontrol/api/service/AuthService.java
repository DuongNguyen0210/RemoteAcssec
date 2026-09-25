package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.common.ApiResponse;
import com.remotecontrol.api.dto.auth.LoginData;
import com.remotecontrol.api.dto.auth.LoginRequest;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import com.remotecontrol.api.util.JwtUtil;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;

import java.util.Objects;
import java.util.Optional;

@Service
@RequiredArgsConstructor
public class AuthService {

    private final UserRepository userRepository;
    private final ChildRepository childRepository;
    private final JwtUtil jwtUtil;
    private final PresenceService presenceService;

    public ApiResponse<LoginData> login(LoginRequest request) {
        String username = request.getUsername();
        String password = request.getPassword();

        Optional<User> u = userRepository.findByUsername(username);
        Optional<Child> c = childRepository.findByUsername(username);

        if (u.isPresent() && Objects.equals(u.get().getPassword(), password)) {
            String token = jwtUtil.generateToken(username, "ADMIN", String.valueOf(u.get().getId()));
            LoginData data = LoginData.builder()
                    .token(token)
                    .role("ADMIN")
                    .username(username)
                    .userId(String.valueOf(u.get().getId()))
                    .build();
            return ApiResponse.success("Account Login Successful", data);
        }
        else if (c.isPresent() && com.remotecontrol.api.util.ChildPasswords.matches(password, c.get().getPassword())) {
            String sessionId = java.util.UUID.randomUUID().toString();
            presenceService.openSession(c.get().getId(), sessionId);
            String token = jwtUtil.generateToken(username, "CHILD", String.valueOf(c.get().getId()), sessionId);
            LoginData data = LoginData.builder()
                    .token(token)
                    .role("CHILD")
                    .sessionId(sessionId)
                    .username(username)
                    .userId(String.valueOf(c.get().getId()))
                    .build();
            return ApiResponse.success("Account Login Successful", data);
        }

        return ApiResponse.error("AUTH_FAILED", "Wrong Username or Password");
    }

    public ApiResponse<Void> registerAdmin(LoginRequest request) {
        if (userRepository.existsByUsername(request.getUsername()))
            return ApiResponse.error("USER_ALREADY_EXISTS", "Username already exists");

        User user = User.builder()
                .username(request.getUsername())
                .password(request.getPassword())
                .maxChildren(5)
                .build();
        userRepository.save(user);
        return ApiResponse.success("Registration successful");
    }
}
