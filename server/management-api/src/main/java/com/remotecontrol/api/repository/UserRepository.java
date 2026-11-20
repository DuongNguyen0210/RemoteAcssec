package com.remotecontrol.api.repository;

import com.remotecontrol.api.entity.User;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.Optional;

@Repository
public interface UserRepository extends JpaRepository<User, Long> {
    @org.springframework.transaction.annotation.Transactional
    @org.springframework.data.jpa.repository.Modifying
    @org.springframework.data.jpa.repository.Query("update User e set e.password = :replacement where e.id = :id and e.password = :previous")
    int upgradePassword(@org.springframework.data.repository.query.Param("id") Long id,
            @org.springframework.data.repository.query.Param("previous") String previous,
            @org.springframework.data.repository.query.Param("replacement") String replacement);

    Optional<User> findByUsername(String username);
    boolean existsByUsername(String username);
}
