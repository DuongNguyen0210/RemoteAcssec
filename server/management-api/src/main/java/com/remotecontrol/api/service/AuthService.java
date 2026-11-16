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
    private final LoginLeaseService loginLeases;

    public ApiResponse<LoginData> login(LoginRequest request) {
        String username = request.getUsername();
        String password = request.getPassword();

        Optional<User> u = userRepository.findByUsername(username);
        Optional<Child> c = childRepository.findByUsername(username);

        if (u.isPresent() && Objects.equals(u.get().getPassword(), password)) {
            String sessionId = java.util.UUID.randomUUID().toString();
            if (!loginLeases.claim("ADMIN", u.get().getId().toString(), sessionId, u.get().getPassword()))
                return ApiResponse.error("ACCOUNT_IN_USE", "Account is already signed in");
            String token = jwtUtil.generateToken(username, "ADMIN", String.valueOf(u.get().getId()), sessionId);
            LoginData data = LoginData.builder()
                    .token(token)
                    .role("ADMIN")
                    .sessionId(sessionId)
                    .username(username)
                    .userId(String.valueOf(u.get().getId()))
                    .build();
            return ApiResponse.success("Account Login Successful", data);
        }
        else if (c.isPresent() && com.remotecontrol.api.util.ChildPasswords.matches(password, c.get().getPassword())) {
            String sessionId = java.util.UUID.randomUUID().toString();
            if (!loginLeases.claim("CHILD", c.get().getId().toString(), sessionId, c.get().getPassword()))
                return ApiResponse.error("ACCOUNT_IN_USE", "Account is already signed in");
            try {
                presenceService.openSession(c.get().getId(), sessionId, c.get().getPassword());
            } catch (RuntimeException error) {
                loginLeases.release("CHILD", c.get().getId().toString(), sessionId, c.get().getPassword());
                throw error;
            }
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

    private String credential(String role, String id) {
        if ("ADMIN".equals(role)) return userRepository.findById(Long.valueOf(id)).map(User::getPassword).orElse(null);
        if ("CHILD".equals(role)) return childRepository.findById(Long.valueOf(id)).map(Child::getPassword).orElse(null);
        return null;
    }

    public boolean isSessionActive(String role, String id, String sessionId) {
        boolean active = sessionId != null && loginLeases.active(role, id, sessionId, credential(role, id))
                && (!"CHILD".equals(role) || presenceService.isSessionActive(id, sessionId));
        if (!active) loginLeases.drain(role, id, sessionId);
        return active;
    }

    public boolean renew(com.remotecontrol.api.dto.common.UserPrincipal user) {
        return loginLeases.renew(user.getRole(), user.getId(), user.getSessionId(), credential(user.getRole(), user.getId()));
    }

    public void logout(com.remotecontrol.api.dto.common.UserPrincipal user) {
        loginLeases.release(user.getRole(), user.getId(), user.getSessionId(), credential(user.getRole(), user.getId()));
        if ("CHILD".equals(user.getRole())) childRepository.findById(Long.valueOf(user.getId())).ifPresent(child ->
                presenceService.closeSession(child.getOwner().getId(), user.getSessionId()));
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
