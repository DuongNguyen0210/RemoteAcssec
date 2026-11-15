package com.remotecontrol.api.repository;

import com.remotecontrol.api.entity.User;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.Optional;

@Repository
public interface UserRepository extends JpaRepository<User, Long> {
    Optional<User> findByUsername(String username);
    @org.springframework.data.jpa.repository.Lock(jakarta.persistence.LockModeType.PESSIMISTIC_WRITE)
    @org.springframework.data.jpa.repository.Query("select u from User u where u.username = :username")
    Optional<User> findByUsernameForUpdate(@org.springframework.data.repository.query.Param("username") String username);
    boolean existsByUsername(String username);
}
