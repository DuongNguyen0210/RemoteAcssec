package com.remotecontrol.api.repository;

import com.remotecontrol.api.entity.User;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.Optional;

@Repository
public interface UserRepository extends JpaRepository<User, Long> {
    @org.springframework.transaction.annotation.Transactional
    @org.springframework.data.jpa.repository.Modifying(clearAutomatically = true, flushAutomatically = true)
    @org.springframework.data.jpa.repository.Query("update User e set e.password = :replacement where e.id = :id and e.password = :previous")
    int upgradePassword(@org.springframework.data.repository.query.Param("id") Long id,
            @org.springframework.data.repository.query.Param("previous") String previous,
            @org.springframework.data.repository.query.Param("replacement") String replacement);


    Optional<User> findByUsername(String username);
    @org.springframework.data.jpa.repository.Lock(jakarta.persistence.LockModeType.PESSIMISTIC_WRITE)
    @org.springframework.data.jpa.repository.Query("select u from User u where u.username = :username")
    Optional<User> findByUsernameForUpdate(@org.springframework.data.repository.query.Param("username") String username);
    boolean existsByUsername(String username);

    // ADMIN and CHILD share a login namespace, even though they use separate tables.
    @org.springframework.data.jpa.repository.Query(value =
            "select 1 from pg_advisory_xact_lock(hashtextextended(:username, 38192641))", nativeQuery = true)
    int lockAccountName(@org.springframework.data.repository.query.Param("username") String username);
}
