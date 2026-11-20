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
        if (admin.isPresent() && ChildPasswords.matches(password, admin.get().getPassword())) {
            return openLogin(username, "ADMIN", admin.get().getId(), admin.get().getPassword(), password);
        }
        if (child.isPresent() && ChildPasswords.matches(password, child.get().getPassword())) {
            return openLogin(username, "CHILD", child.get().getId(), child.get().getPassword(), password);
        }
        return ApiResponse.error("AUTH_FAILED", "Wrong Username or Password");
    }

    private ApiResponse<LoginData> openLogin(String username, String role, Long accountId,
                                             String credential, String suppliedPassword) {
        String id = accountId.toString();
        String sessionId = UUID.randomUUID().toString();
        // Claim before migration: a rejected second login must not invalidate the first.
        if (!loginLeases.claim(role, id, sessionId, credential))
            return ApiResponse.error("ACCOUNT_IN_USE", "Account is already signed in");
        boolean issued = false;
        try {
            String boundCredential = credential;
            if (ChildPasswords.needsUpgrade(credential)) {
                String replacement = ChildPasswords.encode(suppliedPassword);
                int updated = "ADMIN".equals(role)
                        ? userRepository.upgradePassword(accountId, credential, replacement)
                        : childRepository.upgradePassword(accountId, credential, replacement);
                if (updated != 1 || !loginLeases.rebindCredential(role, id, sessionId, credential, replacement))
                    return ApiResponse.error("AUTH_RETRY", "Credentials changed; please sign in again");
                boundCredential = replacement;
            }
            // A reset/delete racing the verified snapshot must not issue a usable login.
            if (!Objects.equals(boundCredential, credential(role, id)))
                return ApiResponse.error("AUTH_RETRY", "Credentials changed; please sign in again");
            if ("CHILD".equals(role)) presenceService.openSession(accountId, sessionId, boundCredential);
            String token = jwtUtil.generateToken(username, role, id, sessionId);
            LoginData data = LoginData.builder()
                    .token(token).role(role).sessionId(sessionId).username(username).userId(id).build();
            issued = true;
            return ApiResponse.success("Account Login Successful", data);
        } finally {
            // Compare by the unique owner even if migration already changed its fingerprint.
            if (!issued) loginLeases.releaseSession(role, id, sessionId);
        }
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

    @org.springframework.transaction.annotation.Transactional
    public ApiResponse<Void> registerAdmin(LoginRequest request) {
        String username = request.getUsername();
        String password = request.getPassword();
        // Leave room for a CHILD suffix in the shared 50-character login namespace.
        if (username == null || !username.matches("[A-Za-z0-9_.-]{3,48}")
                || password == null || password.isBlank() || password.length() < 8 || password.length() > 255)
            return ApiResponse.error("INVALID_INPUT", "Invalid account name or password");

        userRepository.lockAccountName(username);
        if (userRepository.existsByUsername(username) || childRepository.findByUsername(username).isPresent())
            return ApiResponse.error("USER_ALREADY_EXISTS", "Username already exists");

        User user = User.builder()
                .username(username)
                .password(ChildPasswords.encode(password))
                .maxChildren(5)
                .build();
        userRepository.save(user);
        return ApiResponse.success("Registration successful");
    }
}
