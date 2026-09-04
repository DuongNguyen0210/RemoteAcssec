package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.*;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Optional;

@Service
@RequiredArgsConstructor
@Slf4j
public class ChildService {

    private final ChildRepository childRepository;
    private final UserRepository userRepository;
    private final PresenceService presenceService;

    public RegisterResponse register(RegisterRequest request, UserPrincipal currentUser) {

        String childUsername = request.getChildUsername();
        String password = request.getPassword();

        Optional<User> user = userRepository.findByUsername(currentUser.getUsername());
        if(!user.isPresent())
            return  new RegisterResponse(false, "Not found", childUsername, password);

        Optional<Child> child = childRepository.findByChildUsername(currentUser.getUsername() + childUsername);
        if (child.isPresent())
            return new RegisterResponse(false, "Child already exists", childUsername, password);

        addChild(childUsername, password, user.get());
        return new RegisterResponse(true, "Accepted", currentUser.getUsername() + childUsername, password);
    }

    public ListChillResponse getListChillResponse(UserPrincipal currenUser)
    {
        if(!currenUser.getRole().equals("ADMIN"))
            return new ListChillResponse(Collections.emptyList(), false, "Can not to access");

        Optional<User> user = userRepository.findByUsername(currenUser.getUsername());
        if(!user.isPresent())
            return new ListChillResponse(Collections.emptyList(), false, "Not found user");

        List<Child> child = childRepository.findByOwner(user.get());
        List<ChildDto> childList = new ArrayList<>();
        for (Child c : child) {
            boolean isOnline = presenceService.isDeviceOnline(user.get().getUsername(), c.getUsername());
            childList.add(new ChildDto(c.getUsername(), c.getPassword(), isOnline));
        }

        return new ListChillResponse(childList, true, "Accepted");
    }

    public void addChild(String childUsername, String password, User user) {
            Child newChild = Child.builder()
                    .username(user.getUsername() + childUsername)
                    .password(password)
                    .owner(user)
                    .build();
            childRepository.save(newChild);
    }

    public boolean handleHeartbeat(UserPrincipal currentUser, InfoPrincipal currentInfo) {
        if (!"CHILD".equals(currentUser.getRole()))
            return false;

        Optional<Child> c = childRepository.findByChildUsername(currentUser.getUsername());
        if (!c.isPresent()) {
            return false;
        }

        Child child = c.get();
        User parent = child.getOwner();
        presenceService.markDeviceOnline(parent, currentUser, currentInfo);
        return true;
    }
}

