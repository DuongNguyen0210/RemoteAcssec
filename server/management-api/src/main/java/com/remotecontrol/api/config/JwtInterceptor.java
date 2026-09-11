package com.remotecontrol.api.config;

import com.remotecontrol.api.dto.common.InfoPrincipal;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.util.JwtUtil;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;

@Component
@RequiredArgsConstructor
public class JwtInterceptor implements HandlerInterceptor {

    private final JwtUtil jwtUtil;

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) throws Exception {
        String authHeader = request.getHeader("Authorization");
        if (authHeader == null || !authHeader.startsWith("Bearer ")) {
            response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            response.setContentType("application/json;charset=UTF-8");
            response.getWriter().write("{\"success\":false,\"errorCode\":\"UNAUTHORIZED\",\"message\":\"Need Bearer Token\",\"data\":null}");
            return false;
        }

        String token = authHeader.substring(7);
        try {
            response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            if (jwtUtil.isTokenExpired(token)) {
                response.setContentType("application/json;charset=UTF-8");
                response.getWriter().write("{\"success\":false,\"errorCode\":\"TOKEN_EXPIRED\",\"message\":\"Token Expired\",\"data\":null}");
                return false;
            }

            String username = jwtUtil.extractUsername(token);
            String role = jwtUtil.extractRole(token);
            String id = jwtUtil.extractId(token);
            UserPrincipal currentUser = UserPrincipal.builder()
                    .id(id)
                    .username(username)
                    .role(role)
                    .build();
            request.setAttribute("currentUser", currentUser);

            String ip = request.getHeader("X-Forwarded-For");
            if (ip == null || ip.isEmpty() || "unknown".equalsIgnoreCase(ip)) {
                ip = request.getRemoteAddr();
            } else {
                ip = ip.split(",")[0].trim();
            }

            InfoPrincipal currentInfo = InfoPrincipal.builder()
                    .ip(ip)
                    .build();
            request.setAttribute("currentInfo", currentInfo);

            return true;
        } catch (Exception e) {
            response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            response.setContentType("application/json;charset=UTF-8");
            response.getWriter().write("{\"success\":false,\"errorCode\":\"INVALID_TOKEN\",\"message\":\"Invalid Token\",\"data\":null}");
            return false;
        }
    }
}
