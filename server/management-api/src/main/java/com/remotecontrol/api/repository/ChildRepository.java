package com.remotecontrol.api.repository;

import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface ChildRepository extends JpaRepository<Child, Long> {
    Optional<Child> findByUsername(String childUsername);
    List<Child> findByOwner(User owner);
}
