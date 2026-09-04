package com.remotecontrol.api.dto;

import lombok.AllArgsConstructor;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@NoArgsConstructor
@AllArgsConstructor
public class ChildDto {
    private String username;
    private String childUsername;
    private String password;
    private Boolean online;

    public ChildDto(String username, String password) {
        this.username = username;
        this.childUsername = username;
        this.password = password;
        this.online = false;
    }

    public ChildDto(String username, String password, Boolean online) {
        this.username = username;
        this.childUsername = username;
        this.password = password;
        this.online = online;
    }
}
