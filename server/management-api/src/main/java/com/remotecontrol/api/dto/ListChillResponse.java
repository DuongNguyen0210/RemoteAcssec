package com.remotecontrol.api.dto;

import lombok.Data;
import lombok.NoArgsConstructor;
import lombok.AllArgsConstructor;

import java.util.List;

@Data
@NoArgsConstructor
@AllArgsConstructor
public class ListChillResponse {
    private List<ChildDto> children;
    private Boolean success;
    private String message;
}
