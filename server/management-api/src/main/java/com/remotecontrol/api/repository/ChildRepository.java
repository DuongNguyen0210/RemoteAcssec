package com.remotecontrol.api.repository;

import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface ChildRepository extends JpaRepository<Child, Long> {
    @org.springframework.transaction.annotation.Transactional
    @org.springframework.data.jpa.repository.Modifying
    @org.springframework.data.jpa.repository.Query("update Child e set e.password = :replacement where e.id = :id and e.password = :previous")
    int upgradePassword(@org.springframework.data.repository.query.Param("id") Long id,
            @org.springframework.data.repository.query.Param("previous") String previous,
            @org.springframework.data.repository.query.Param("replacement") String replacement);

    Optional<Child> findByUsername(String childUsername);
    List<Child> findByOwner(User owner);
}
