package com.remotecontrol.api.dto.child;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;
import lombok.Getter;
import lombok.Setter;

@Getter
@Setter
public class UpdateChildRequest {
    @NotBlank
    @Size(max = 50)
    private String childUsername;
    @Size(max = 255)
    private String newPassword;
}
