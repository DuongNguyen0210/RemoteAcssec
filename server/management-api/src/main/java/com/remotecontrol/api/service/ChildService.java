package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.RegisterRequest;
import com.remotecontrol.api.dto.RegisterResponse;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import com.remotecontrol.api.util.JwtUtil;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;

import java.util.Optional;

@Service
@RequiredArgsConstructor
public class ChildService {

    private final ChildRepository childRepository;
    private final UserRepository userRepository;
    private final JwtUtil jwtUtil;

    public RegisterResponse register(RegisterRequest request) {

        String childUsername = request.getChildUsername();
        String password = request.getPassword();
        String token = request.getToken();

        try {
            boolean oke = !jwtUtil.isTokenExpired(token);
            if(!oke)
                return new RegisterResponse(false, "Token expired", childUsername, password);

            if (!"ADMIN".equals(jwtUtil.extractRole(token)))
                return new RegisterResponse(false, "Dont Accepted", childUsername, password);

            String username = jwtUtil.extractUsername(token);
            Optional<User> user = userRepository.findByUsername(username);
            if(!user.isPresent())
                return new RegisterResponse(false, "User not found", childUsername, password);


            Optional<Child> child = childRepository.findByChildUsername(username + childUsername);
            if(child.isPresent())
                return new RegisterResponse(false, "Child already exists", childUsername, password);
            else {
                addChild(childUsername, password, user.get());
                return new RegisterResponse(true, "Accepted", username + childUsername, password);
            }
        }
        catch (Exception e) {
            return new RegisterResponse(false, "Invalid token", childUsername, password);
        }
    }

    public void addChild(String childUsername, String password, User user) {
        Child newChild = Child.builder()
                .childUsername(user.getUsername() + childUsername)
                .password(password)
                .owner(user)
                .build();
        childRepository.save(newChild);
    }
}

