package com.remotecontrol.api.dto;

import lombok.Data;
import lombok.RequiredArgsConstructor;

@Data
@RequiredArgsConstructor
public class ChildSummaryResponse {
    private final String childUsername;
}
