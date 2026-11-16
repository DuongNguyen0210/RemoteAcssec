package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.common.ApiResponse;
import com.remotecontrol.api.dto.auth.LoginData;
import com.remotecontrol.api.dto.auth.LoginRequest;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import com.remotecontrol.api.util.JwtUtil;
import com.remotecontrol.api.util.ChildPasswords;
import com.remotecontrol.api.dto.common.UserPrincipal;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;

import java.util.Objects;
import java.util.Optional;
import java.util.UUID;

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

        Optional<User> admin = userRepository.findByUsername(username);
        Optional<Child> child = childRepository.findByUsername(username);
        if (admin.isPresent() && Objects.equals(admin.get().getPassword(), password)) {
            return openLogin(username, "ADMIN", admin.get().getId(), admin.get().getPassword());
        }
        if (child.isPresent() && ChildPasswords.matches(password, child.get().getPassword())) {
            return openLogin(username, "CHILD", child.get().getId(), child.get().getPassword());
        }
        return ApiResponse.error("AUTH_FAILED", "Wrong Username or Password");
    }

    private ApiResponse<LoginData> openLogin(String username, String role, Long accountId, String credential) {
        String id = accountId.toString();
        String sessionId = UUID.randomUUID().toString();
        if (!loginLeases.claim(role, id, sessionId, credential))
            return ApiResponse.error("ACCOUNT_IN_USE", "Account is already signed in");
        if ("CHILD".equals(role)) {
            try {
                presenceService.openSession(accountId, sessionId, credential);
            } catch (RuntimeException error) {
                loginLeases.release(role, id, sessionId, credential);
                throw error;
            }
        }
        String token = jwtUtil.generateToken(username, role, id, sessionId);
        LoginData data = LoginData.builder()
                .token(token).role(role).sessionId(sessionId).username(username).userId(id).build();
        return ApiResponse.success("Account Login Successful", data);
    }

    private String credential(String role, String id) {
        if ("ADMIN".equals(role)) return userRepository.findById(Long.valueOf(id)).map(User::getPassword).orElse(null);
        if ("CHILD".equals(role)) return childRepository.findById(Long.valueOf(id)).map(Child::getPassword).orElse(null);
        return null;
    }

    public boolean isSessionActive(String role, String id, String sessionId) {
        // CHILD presence validation already checks both credential binding and login lease.
        boolean active = sessionId != null && ("CHILD".equals(role)
                ? presenceService.isSessionActive(id, sessionId)
                : loginLeases.active(role, id, sessionId, credential(role, id)));
        if (!active) loginLeases.drain(role, id, sessionId);
        return active;
    }

    public boolean renew(UserPrincipal user) {
        return loginLeases.renew(user.getRole(), user.getId(), user.getSessionId(), credential(user.getRole(), user.getId()));
    }

    public void logout(UserPrincipal user) {
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
