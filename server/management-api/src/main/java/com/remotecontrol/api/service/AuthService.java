package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.LoginRequest;
import com.remotecontrol.api.dto.LoginResponse;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;

import java.util.Objects;
import java.util.Optional;

@Service
@RequiredArgsConstructor
public class AuthService {

    private final UserRepository userRepository;
    private final ChildRepository childRepository;

    public LoginResponse login(LoginRequest request) {
        String username = request.getUsername();
        String password = request.getPassword();
        LoginResponse l = new LoginResponse();
        l.setSuccess(false);
        l.setMessage("Wrong Username or Password");
        Optional<User> u = userRepository.findByUsername(username);
        Optional<Child> c = childRepository.findByChildUsername(username);
        if(u.isPresent() && Objects.equals(u.get().getPassword(), password)) {
            l.setSuccess(true);
            l.setRole("ADMIN");
            l.setMessage("Account Login Successful");
        }
        else if(c.isPresent() && Objects.equals(c.get().getPassword(), password))
        {
            l.setSuccess(true);
            l.setRole("CHILD");
            l.setMessage("Account Login Successful");
        }
        return l;
    }
}
