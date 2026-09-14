package com.remotecontrol.api.config;

import com.remotecontrol.api.annotation.RequireRole;
import com.remotecontrol.api.dto.common.InfoPrincipal;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.util.JwtUtil;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Component;
import org.springframework.web.method.HandlerMethod;
import org.springframework.web.servlet.HandlerInterceptor;

import com.remotecontrol.api.service.PresenceService;
import org.springframework.dao.DataAccessException;
import java.util.Arrays;
import java.util.List;

@Component
@RequiredArgsConstructor
public class JwtInterceptor implements HandlerInterceptor {

    private final JwtUtil jwtUtil;
    private final PresenceService presenceService;

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
            if (jwtUtil.isTokenExpired(token)) {
                response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
                response.setContentType("application/json;charset=UTF-8");
                response.getWriter().write("{\"success\":false,\"errorCode\":\"TOKEN_EXPIRED\",\"message\":\"Token Expired\",\"data\":null}");
                return false;
            }

            String username = jwtUtil.extractUsername(token);
            String role = jwtUtil.extractRole(token);
            String id = jwtUtil.extractId(token);
            String sessionId = jwtUtil.extractSessionId(token);
            if ("CHILD".equals(role) && !presenceService.isSessionActive(id, sessionId)) {
                response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
                response.setContentType("application/json;charset=UTF-8");
                response.getWriter().write("{\"success\":false,\"errorCode\":\"SESSION_EXPIRED\",\"message\":\"Please sign in again\"}");
                return false;
            }
            UserPrincipal currentUser = UserPrincipal.builder()
                    .id(id)
                    .username(username)
                    .role(role)
                    .sessionId(sessionId)
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

            if (handler instanceof HandlerMethod handlerMethod) {
                RequireRole requireRole = handlerMethod.getMethodAnnotation(RequireRole.class);
                if (requireRole == null) {
                    requireRole = handlerMethod.getBeanType().getAnnotation(RequireRole.class);
                }

                if (requireRole != null) {
                    List<String> allowedRoles = Arrays.asList(requireRole.value());
                    if (currentUser == null || !allowedRoles.contains(currentUser.getRole())) {
                        response.setStatus(HttpServletResponse.SC_FORBIDDEN);
                        response.setContentType("application/json;charset=UTF-8");
                        response.getWriter().write("{\"success\":false,\"errorCode\":\"FORBIDDEN\",\"message\":\"Access denied: insufficient permissions\",\"data\":null}");
                        return false;
                    }
                }
            }

            return true;
        } catch (DataAccessException e) {
            response.setStatus(HttpServletResponse.SC_SERVICE_UNAVAILABLE);
            return false;
        } catch (Exception e) {
            response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            response.setContentType("application/json;charset=UTF-8");
            response.getWriter().write("{\"success\":false,\"errorCode\":\"INVALID_TOKEN\",\"message\":\"Invalid Token\",\"data\":null}");
            return false;
        }
    }
}
