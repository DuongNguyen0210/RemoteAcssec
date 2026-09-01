package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.ChildDto;
import com.remotecontrol.api.dto.ListChillResponse;
import com.remotecontrol.api.dto.RegisterRequest;
import com.remotecontrol.api.dto.RegisterResponse;
import com.remotecontrol.api.dto.HeartbeatRequest;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import com.remotecontrol.api.util.JwtUtil;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;

@Service
@RequiredArgsConstructor
@Slf4j
public class ChildService {

    private final ChildRepository childRepository;
    private final UserRepository userRepository;
    private final JwtUtil jwtUtil;

    public RegisterResponse register(RegisterRequest request, String token) {
        String childUsername = request.getChildUsername();
        String password = request.getPassword();
        log.info("Token: {}", token);
        if (token == null || token.isEmpty()) {
            return new RegisterResponse(false, "Missing token", childUsername, password);
        }

        Optional<User> user;
        String username;

        try {
            boolean oke = !jwtUtil.isTokenExpired(token);
            if (!oke)
                return new RegisterResponse(false, "Token expired", childUsername, password);

            if (!"ADMIN".equals(jwtUtil.extractRole(token)))
                return new RegisterResponse(false, "Dont Accepted", childUsername, password);

            username = jwtUtil.extractUsername(token);
            user = userRepository.findByUsername(username);
            if (!user.isPresent())
                return new RegisterResponse(false, "User not found", childUsername, password);


            Optional<Child> child = childRepository.findByChildUsername(username + childUsername);
            if (child.isPresent())
                return new RegisterResponse(false, "Child already exists", childUsername, password);
        } catch (Exception e) {
            return new RegisterResponse(false, "Invalid token", childUsername, password);
        }
        addChild(childUsername, password, user.get());
        return new RegisterResponse(true, "Accepted", username + childUsername, password);
    }

    public ListChillResponse getListChillResponse(String token)
    {
        if (token == null || token.isEmpty()) {
            return new ListChillResponse(Collections.emptyList(), false, "Thiếu token");
        }

        String role, username;
        Optional<User> user;
        try {
            role = jwtUtil.extractRole(token);
            username = jwtUtil.extractUsername(token);
            user = userRepository.findByUsername(username);
            if(!user.isPresent())
                return new ListChillResponse(Collections.emptyList(), false, "User không tồn tại");
            if(!role.equals("ADMIN"))
                return new ListChillResponse(Collections.emptyList(), false, "Không có quyền truy cập");
        } catch (Exception e) {
            return new ListChillResponse(Collections.emptyList(), false, "Lỗi xác thực");
        }

        List<Child> childEntities = childRepository.findByOwner(user.get());
        List<ChildDto> childList = new ArrayList<>();
        for (Child c : childEntities) {
            childList.add(new ChildDto(c.getChildUsername(), c.getPassword()));
        }

        return new ListChillResponse(childList, true, "Accepted");
    }

    public void addChild(String childUsername, String password, User user) {
            Child newChild = Child.builder()
                    .childUsername(user.getUsername() + childUsername)
                    .password(password)
                    .owner(user)
                    .build();
            childRepository.save(newChild);
    }

    public boolean handleHeartbeat(String token, HeartbeatRequest request, String remoteIp) {
        if (token == null || token.isEmpty()) {
            return false;
        }

        try {
            if (jwtUtil.isTokenExpired(token)) {
                return false;
            }

            if (!"CHILD".equals(jwtUtil.extractRole(token))) {
                return false;
            }

            String childUsername = jwtUtil.extractUsername(token);
            Optional<Child> childOpt = childRepository.findByChildUsername(childUsername);

            if (!childOpt.isPresent()) {
                return false;
            }

            return true;
        } catch (Exception e) {
            log.error("Invalid token in heartbeat: {}", e.getMessage());
            return false;
        }
    }
}

