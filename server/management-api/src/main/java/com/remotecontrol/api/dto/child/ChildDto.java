package com.remotecontrol.api.dto.child;

import lombok.*;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class ChildDto {
    private Long id;
    private String username;
    private String childUsername;
}
