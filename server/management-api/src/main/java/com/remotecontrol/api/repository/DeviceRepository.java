package com.remotecontrol.api.repository;

import com.remotecontrol.api.entity.Device;
import com.remotecontrol.api.entity.User;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface DeviceRepository extends JpaRepository<Device, Long> {
    Optional<Device> findByDeviceUid(String deviceUid);
    List<Device> findByOwner(User owner);
    List<Device> findByOwnerId(Long ownerId);
    List<Device> findByStatus(String status);
}
