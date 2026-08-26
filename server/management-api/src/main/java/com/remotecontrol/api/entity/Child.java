package com.remotecontrol.api.entity;

import jakarta.persistence.*;
import lombok.*;

import java.time.LocalDateTime;

@Entity
@Table(name = "child")
@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class Child {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "child_username", nullable = false, unique = true, length = 50)
    private String childUsername;

    @Column(nullable = false)
    private String password;

    @Column(nullable = false, length = 100)
    private String name;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "user_id", nullable = false)
    @ToString.Exclude
    private User owner;

    @Column(length = 45)
    private String ipAddress;

    @Column(nullable = false)
    @Builder.Default
    private String status = "OFFLINE";

    @Column(name = "last_seen")
    private LocalDateTime lastSeen;
}
