package com.remotecontrol.api.dto.child;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class ChildDto {
    private String username;
    private String childUsername;
    private String password;
    private Boolean online;
    private String ipAddress;
    private String os;
    private String deviceUid;
    private String deviceName;

    public ChildDto(String username, String password, Boolean online) {
        this.username = username;
        this.childUsername = username;
        this.password = password;
        this.online = online;
    }
}
