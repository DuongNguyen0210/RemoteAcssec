FROM maven:3.9.9-eclipse-temurin-17 AS build
WORKDIR /build
COPY server/pom.xml pom.xml
COPY server/management-api management-api
COPY server/relay-proxy/pom.xml relay-proxy/pom.xml
RUN mvn -B -pl management-api package spring-boot:repackage -DskipTests

FROM eclipse-temurin:17-jre-jammy
RUN apt-get update && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/* && useradd --system --uid 10001 app
WORKDIR /app
COPY --from=build /build/management-api/target/management-api-1.0.0-SNAPSHOT.jar app.jar
USER 10001
EXPOSE 9090
ENTRYPOINT ["java", "-jar", "/app/app.jar"]
