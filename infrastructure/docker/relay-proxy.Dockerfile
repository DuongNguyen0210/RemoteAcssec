FROM maven:3.9.9-eclipse-temurin-17 AS build
WORKDIR /build
COPY server/pom.xml pom.xml
COPY server/management-api/pom.xml management-api/pom.xml
COPY server/relay-proxy relay-proxy
RUN mvn -B -pl relay-proxy -am package -DskipTests
RUN mvn -B -pl relay-proxy dependency:copy-dependencies -DincludeScope=runtime

FROM eclipse-temurin:17-jre-jammy
RUN useradd --system --uid 10001 app
WORKDIR /app
COPY --from=build /build/relay-proxy/target/relay-proxy-1.0.0-SNAPSHOT.jar app.jar
COPY --from=build /build/relay-proxy/target/dependency/ lib/
USER 10001
EXPOSE 8080
ENTRYPOINT ["java", "-cp", "/app/app.jar:/app/lib/*", "com.remotecontrol.relay.server.RelayServer"]
