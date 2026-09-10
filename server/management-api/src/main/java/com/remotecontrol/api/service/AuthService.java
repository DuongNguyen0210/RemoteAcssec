package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.ApiResponse;
import com.remotecontrol.api.dto.LoginData;
import com.remotecontrol.api.dto.LoginRequest;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;

import java.util.Objects;
import java.util.Optional;
import com.remotecontrol.api.util.JwtUtil;

@Service
@RequiredArgsConstructor
public class AuthService {

    private final UserRepository userRepository;
    private final ChildRepository childRepository;
    private final JwtUtil jwtUtil;

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
        } else if (c.isPresent() && Objects.equals(c.get().getPassword(), password)) {
            String token = jwtUtil.generateToken(username, "CHILD", String.valueOf(c.get().getId()));
            LoginData data = LoginData.builder()
                    .token(token)
                    .role("CHILD")
                    .username(username)
                    .userId(String.valueOf(c.get().getId()))
                    .build();
            return ApiResponse.success("Account Login Successful", data);
        }

        return ApiResponse.error("Wrong Username or Password");
    }
}
