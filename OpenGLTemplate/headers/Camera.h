﻿#ifndef CAMERA_H
#define CAMERA_H

#include <core.h>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 2.0f;
const float SENSITIVITY = 0.06f;
const float ZOOM = 45.0f;

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    float Yaw;
    float Pitch;
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;
    bool isLookingAtEmptySpace;
    bool isMoving;

    float minSpeed = 0.2f;
    float maxSpeed = 3.0f;

    float speedFactor = 1.0f;

    float scrollMomentum = 0.5f;
    float maxScrollVelocity = 3.0f;
    float scrollDeceleration = 5.0f;

    bool useSmoothScrolling = true;
    float scrollVelocity = 0.0f;

    glm::vec3 OrbitPoint;
    float OrbitDistance;
    bool IsOrbiting;
    bool IsPanning;

    // Animation properties
    bool IsAnimating;
    glm::vec3 AnimationStartPosition;
    glm::vec3 AnimationEndPosition;
    glm::vec3 AnimationStartFront;
    glm::vec3 AnimationEndFront;
    float AnimationProgress;
    float AnimationDuration;

    bool useNewMethod = true;  // For stereo rendering method
    bool wireframe = false;    // For wireframe mode

    bool zoomToCursor = false;
    glm::vec3 cursorPosition = glm::vec3(0.0f);
    bool cursorValid = false;
    glm::vec3 scrollTargetPos = glm::vec3(0.0f);
    bool isScrollingToCursor = false;

    float distanceToNearestObject = 0.0f;
    bool distanceUpdated = false;

    bool orbitAroundCursor = false;


    std::function<void()> centeringCompletedCallback;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH)
        : isMoving(false), isLookingAtEmptySpace(false), Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM),
        IsOrbiting(false), IsPanning(false), IsAnimating(false), AnimationProgress(0.0f), AnimationDuration(0.5f), centeringCompletedCallback(nullptr) // Reduced duration to 0.5 seconds
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        OrbitPoint = Position + Front;
        OrbitDistance = 1.0f;

        updateCameraVectors();
    }

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(Position, Position + Front, Up);
    }

    glm::mat4 GetProjectionMatrix(float aspectRatio, float nearPlane, float farPlane) const {
        return glm::perspective(glm::radians(Zoom), aspectRatio, nearPlane, farPlane);
    }

    void UpdateCursorInfo(const glm::vec3& pos, bool valid) {
        cursorPosition = pos;
        cursorValid = valid;
    }

    void UpdateDistanceToObject(float distance) {
        distanceToNearestObject = distance;
        distanceUpdated = true;

    }
    // Function for stereo projection
    glm::mat4 offsetProjection(glm::mat4& centerProjection, float separation, float convergence) const {
        glm::mat4 o = glm::mat4(centerProjection);
        o[2][0] = o[2][0] - separation;
        o[3][0] = o[3][0] - separation * convergence;
        return o;
    }

    bool isInFrustum(const glm::vec3& point, float radius, glm::mat4 viewProj) const {
        for (int i = 0; i < 6; ++i) {
            glm::vec4 plane(
                viewProj[0][3] + (i % 2 == 0 ? viewProj[0][i / 2] : -viewProj[0][i / 2]),
                viewProj[1][3] + (i % 2 == 0 ? viewProj[1][i / 2] : -viewProj[1][i / 2]),
                viewProj[2][3] + (i % 2 == 0 ? viewProj[2][i / 2] : -viewProj[2][i / 2]),
                viewProj[3][3] + (i % 2 == 0 ? viewProj[3][i / 2] : -viewProj[3][i / 2])
            );
            plane /= glm::length(glm::vec3(plane));

            if (glm::dot(point, glm::vec3(plane)) + plane.w <= -radius) {
                return false;
            }
        }
        return true;
    }

    void ProcessKeyboard(Camera_Movement direction, float deltaTime) {
        if (IsAnimating) return; // Disable keyboard movement during animation

        float velocity = MovementSpeed * deltaTime;
        glm::vec3 oldPosition = Position;

        switch (direction) {
        case FORWARD:
            Position += Front * velocity;
            break;
        case BACKWARD:
            Position -= Front * velocity;
            break;
        case LEFT:
            Position -= Right * velocity;
            break;
        case RIGHT:
            Position += Right * velocity;
            break;
        case UP:
            Position += Up * velocity;
            break;
        case DOWN:
            Position -= Up * velocity;
            break;
        }

        glm::vec3 actualMovement = Position - oldPosition;
        isMoving = glm::length(actualMovement) > 0.0001f; // Set a small threshold

        // Update orbit point when moving
        OrbitPoint = Position + Front * OrbitDistance;
    }

    void AdjustMovementSpeed(float distanceToNearestObject, float modelSize, float farPlane) {
        if (!isMoving) return; // Only adjust speed when moving

        // Adjust min and max distances based on model size
        float minDistance = modelSize * 0.1f;
        float maxDistance = modelSize * 10.0f;

        maxSpeed = modelSize * 1.5 * speedFactor; // Should be initialized once at the start
        minSpeed = modelSize * 0.1 * speedFactor; // Should be initialized once at the start

        // Ensure minDistance is not too small
        minDistance = glm::max(minDistance, 0.01f);

        // Ensure maxDistance is sufficiently larger than minDistance
        maxDistance = glm::max(maxDistance, minDistance * 10.0f);

        // Normalize the distance
        float normalizedDistance = (distanceToNearestObject - minDistance) / (maxDistance - minDistance);
        normalizedDistance = glm::clamp(normalizedDistance, 0.0f, 1.0f);

        // Apply logarithmic function
        float logFactor = 4.0f; // Adjust this to change the steepness of the curve, 4 appears to be a good value
        float t = glm::log(1 + normalizedDistance * (exp(logFactor) - 1)) / logFactor;

        // Calculate new target speed
        float newTargetSpeed = minSpeed + t * (maxSpeed - minSpeed);
        newTargetSpeed = glm::clamp(newTargetSpeed, minSpeed, maxSpeed);

        isLookingAtEmptySpace = (distanceToNearestObject == farPlane);

        if (isLookingAtEmptySpace) {
            // Transitioning from object to void, start gradual increase
            float newSpeed = MovementSpeed += MovementSpeed / 50.0f;
            newSpeed = glm::clamp(newSpeed, minSpeed, maxSpeed);
            MovementSpeed = newSpeed;

        }
        else {

            if (newTargetSpeed > MovementSpeed) {
                // Transition to new Speed. this is important if looking from a near object to a far away object(not the far plane) through a gap for example
                MovementSpeed += MovementSpeed / 50;
            }
            else {
                // Set speed instantly if slower
                MovementSpeed = newTargetSpeed;
            }

        }

         /*                         //Debug to fine-tune parameters
        std::cout << "Model size: " << modelSize << ", Distance: " << distanceToNearestObject
            << ", Is looking at empty space: " << (isLookingAtEmptySpace ? "Yes" : "No")
            << ", Current movement speed: " << MovementSpeed << std::endl;*/
    }

    float CalculateScrollFactor(float modelSize) {
        if (!distanceUpdated) {
            return 1.0f; // Default multiplier when distance isn't available
        }

        // Use same parameters as AdjustMovementSpeed for consistency
        float minDistance = modelSize * 0.1f;
        float maxDistance = modelSize * 10.0f;

        // Ensure minDistance is not too small
        minDistance = glm::max(minDistance, 0.01f);

        // Ensure maxDistance is sufficiently larger than minDistance
        maxDistance = glm::max(maxDistance, minDistance * 10.0f);

        // Normalize the distance
        float normalizedDistance = (distanceToNearestObject - minDistance) / (maxDistance - minDistance);
        normalizedDistance = glm::clamp(normalizedDistance, 0.0f, 1.0f);

        // Apply logarithmic function using the same logFactor as movement speed
        float logFactor = 4.0f;
        float t = glm::log(1 + normalizedDistance * (exp(logFactor) - 1)) / logFactor;

        // Calculate scroll factor - similar to speed calculation but with scroll-specific multipliers
        float minScrollFactor = 0.1f;
        float maxScrollFactor = 3.0f;

        float scrollFactor = minScrollFactor + t * (maxScrollFactor - minScrollFactor);

        // If looking at empty space, gradually increase factor
        if (isLookingAtEmptySpace) {
            scrollFactor *= 1.5f; // Slightly more aggressive than movement speed
        }

        return scrollFactor;
    }


    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        if (IsAnimating) return; // Disable mouse movement during animation

        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        if (IsOrbiting) {
            if (orbitAroundCursor) {
                // Pure orbit around cursor implementation

                // Store initial values to calculate relative changes
                glm::vec3 initialPosition = Position;
                glm::vec3 initialFront = Front;
                glm::vec3 initialRight = Right;
                glm::vec3 initialUp = Up;

                // Vector from orbit point to camera
                glm::vec3 orbitToCamera = Position - OrbitPoint;
                float distance = glm::length(orbitToCamera);

                // Calculate rotation quaternions
                glm::quat yawQuat = glm::angleAxis(-glm::radians(xoffset), WorldUp);
                glm::vec3 rightAxis = glm::normalize(glm::cross(orbitToCamera, WorldUp));
                glm::quat pitchQuat = glm::angleAxis(-glm::radians(yoffset), rightAxis);

                // Apply rotations to orbit-to-camera vector
                orbitToCamera = pitchQuat * (yawQuat * orbitToCamera);

                // Normalize and scale back to original distance
                orbitToCamera = glm::normalize(orbitToCamera) * distance;

                // Update camera position
                Position = OrbitPoint + orbitToCamera;

                // Calculate camera rotation to maintain orientation relative to orbit path
                // This is the key part - we rotate the basis vectors by the same amount
                // as we rotated the orbit-to-camera vector
                Front = glm::normalize(pitchQuat * (yawQuat * initialFront));
                Right = glm::normalize(pitchQuat * (yawQuat * initialRight));
                Up = glm::normalize(pitchQuat * (yawQuat * initialUp));

                // Update Yaw and Pitch for consistency, though we don't use them directly
                Yaw += xoffset;
                Pitch += yoffset;

                if (constrainPitch) {
                    if (Pitch > 89.0f) Pitch = 89.0f;
                    if (Pitch < -89.0f) Pitch = -89.0f;
                }
            }
            else {
                // Original standard orbiting behavior

                // Convert to radians
                float yawRad = glm::radians(xoffset);
                float pitchRad = glm::radians(yoffset);

                // Calculate the vector from orbit point to camera
                glm::vec3 toCamera = Position - OrbitPoint;

                // Rotate around world up vector for yaw
                toCamera = glm::rotate(toCamera, -yawRad, WorldUp);

                // Rotate around right vector for pitch
                glm::vec3 right = glm::normalize(glm::cross(toCamera, WorldUp));
                toCamera = glm::rotate(toCamera, -pitchRad, right);

                // Update camera position
                Position = OrbitPoint + toCamera;

                // Point camera at orbit point
                Front = glm::normalize(OrbitPoint - Position);

                // Update Yaw and Pitch
                Yaw += xoffset;
                Pitch += yoffset;

                if (constrainPitch) {
                    if (Pitch > 89.0f) Pitch = 89.0f;
                    if (Pitch < -89.0f) Pitch = -89.0f;
                }

                // Recalculate Right and Up vectors
                Right = glm::normalize(glm::cross(Front, WorldUp));
                Up = glm::normalize(glm::cross(Right, Front));
            }
        }
        else if (IsPanning) {
            glm::vec3 right = glm::normalize(glm::cross(Front, WorldUp));
            Position += right * xoffset * -0.02f;
            Position += WorldUp * yoffset * -0.02f;

            // Update orbit point when panning
            OrbitPoint = Position + Front * OrbitDistance;
        }
        else {
            // Non-orbiting mouse movement 
            Yaw += xoffset;
            Pitch += yoffset;

            if (constrainPitch) {
                if (Pitch > 89.0f) Pitch = 89.0f;
                if (Pitch < -89.0f) Pitch = -89.0f;
            }

            updateCameraVectors();
        }
    }

    void ProcessMouseScroll(float yoffset) {
        if (IsAnimating) return;

        // Calculate base scroll amount using model size and distance
        float modelSize = 1.0f; // This should be passed from outside or stored in the camera
        float scrollFactor = CalculateScrollFactor(modelSize);
        float scaledYOffset = yoffset * scrollFactor;

        if (!useSmoothScrolling) {
            // Direct movement
            if (zoomToCursor && cursorValid) {
                // Calculate direction to cursor
                glm::vec3 dirToCursor = cursorPosition - Position;
                float distance = glm::length(dirToCursor);

                // Only use zoom to cursor if the cursor is at a reasonable distance
                if (distance > 0.01f) {
                    // Normalize direction
                    dirToCursor = glm::normalize(dirToCursor);

                    // Move toward cursor if scrolling forward, away if scrolling backward
                    // Apply scroll factor to make movement distance-dependent
                    Position += dirToCursor * scaledYOffset * MovementSpeed * 0.1f;
                }
                else {
                    // Fallback to standard scrolling
                    Position += Front * scaledYOffset * MovementSpeed * 0.1f;
                }
            }
            else {
                // Original behavior - move along Front vector
                Position += Front * scaledYOffset * MovementSpeed * 0.1f;
            }

            if (IsOrbiting) {
                OrbitPoint = Position + Front * OrbitDistance;
            }
            return;
        }

        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastScrollTime;
        lastScrollTime = currentTime;

        // Apply scroll factor to momentum as well
        scrollVelocity += scaledYOffset * scrollMomentum;
        scrollVelocity = glm::clamp(scrollVelocity, -maxScrollVelocity, maxScrollVelocity);

        // Store cursor position for smooth scrolling if zoomToCursor is enabled
        if (zoomToCursor && cursorValid) {
            scrollTargetPos = cursorPosition;
            isScrollingToCursor = true;
        }
        else {
            isScrollingToCursor = false;
        }
    }

    // Updated UpdateScrolling method
    void UpdateScrolling(float deltaTime) {
        if (scrollVelocity != 0.0f) {
            // Calculate scroll factor for this frame
            float modelSize = 1.0f; // This should be passed from outside or stored in the camera
            float scrollFactor = CalculateScrollFactor(modelSize);

            // Apply scroll factor to velocity
            float adjustedVelocity = scrollVelocity * scrollFactor;

            // Move camera based on velocity
            if (isScrollingToCursor) {
                // Calculate direction to cursor
                glm::vec3 dirToCursor = scrollTargetPos - Position;
                float distance = glm::length(dirToCursor);

                if (distance > 0.01f) {
                    // Normalize direction
                    dirToCursor = glm::normalize(dirToCursor);

                    // Move along direction based on adjusted velocity
                    Position += dirToCursor * adjustedVelocity * MovementSpeed * deltaTime;
                }
                else {
                    // If we're too close to target, revert to standard scrolling
                    Position += Front * adjustedVelocity * MovementSpeed * deltaTime;
                    isScrollingToCursor = false;
                }
            }
            else {
                // Original behavior - move along Front vector with adjusted velocity
                Position += Front * adjustedVelocity * MovementSpeed * deltaTime;
            }

            // Decelerate
            float deceleration = scrollDeceleration * deltaTime * scrollFactor;
            if (abs(scrollVelocity) <= deceleration) {
                scrollVelocity = 0.0f;
            }
            else {
                scrollVelocity -= glm::sign(scrollVelocity) * deceleration;
            }

            // Update orbit point
            if (IsOrbiting) {
                OrbitPoint = Position + Front * OrbitDistance;
            }
        }
    }

    void SetOrbitPoint(float distance) {
        OrbitDistance = distance;
        OrbitPoint = Position + Front * OrbitDistance;
    }

    void SetOrbitPointDirectly(const glm::vec3& point) {
        OrbitPoint = point;
        OrbitDistance = glm::length(Position - OrbitPoint);
    }

    void StartCenteringAnimation(const glm::vec3& targetPoint) {
        IsAnimating = true;
        AnimationStartPosition = Position;

        // Calculate the initial distance to the cursor
        float initialDistance = glm::length(Position - targetPoint);

        // Calculate the direction from the target point to the new camera position
        glm::vec3 directionToCamera = glm::normalize(Position - targetPoint);

        // Set the end position maintaining the initial distance
        AnimationEndPosition = targetPoint + directionToCamera * initialDistance;

        AnimationStartFront = Front;
        AnimationEndFront = glm::normalize(targetPoint - AnimationEndPosition);

        AnimationProgress = 0.0f;

        // Update the orbit distance to match the initial distance
        OrbitDistance = initialDistance;
    }

    void UpdateAnimation(float deltaTime) {
        if (!IsAnimating) return;

        AnimationProgress += deltaTime / AnimationDuration;

        if (AnimationProgress >= 1.0f) {
            // Animation finished
            Position = AnimationEndPosition;
            Front = AnimationEndFront;
            IsAnimating = false;
            updateCameraVectors();

            // Set the orbit point to be in front of the camera at the OrbitDistance
            OrbitPoint = Position + Front * OrbitDistance;

            // Call the callback if it's set
            if (centeringCompletedCallback) {
                centeringCompletedCallback();
            }
        }
        else {
            // Apply easeOut function to the progress
            float t = easeOutCubic(AnimationProgress);

            // Interpolate position and front vector
            Position = glm::mix(AnimationStartPosition, AnimationEndPosition, t);
            Front = glm::normalize(glm::mix(AnimationStartFront, AnimationEndFront, t));
        }

        // Update other vectors
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));

        // Update Yaw and Pitch
        Pitch = glm::degrees(asin(Front.y));
        Yaw = glm::degrees(atan2(Front.z, Front.x));
    }

    void StartOrbiting(bool useCurrentCursorPosition = false) {
        if (useCurrentCursorPosition && cursorValid) {
            // Just set the orbit point to the cursor position without changing camera position
            OrbitPoint = cursorPosition;
            // Calculate distance from camera to this orbit point
            OrbitDistance = glm::length(Position - OrbitPoint);
        }
        IsOrbiting = true;
    }
    void StopOrbiting() { IsOrbiting = false; }
    void StartPanning() { IsPanning = true; }
    void StopPanning() { IsPanning = false; }


    float getDistanceToNearestObject(const Camera& camera, const glm::mat4& projection, const glm::mat4& view,
        const float farPlane, const int windowWidth, const int windowHeight) const {
        const int numSamples = 9; // 3x3 grid
        const int sampleOffset = 100; // pixels from center
        float minDepth = 1.0f;

        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                float depth;
                int x = windowWidth / 2 + i * sampleOffset;
                int y = windowHeight / 2 + j * sampleOffset;
                glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
                minDepth = std::min(minDepth, depth);
            }
        }

        if (minDepth == 1.0f) {
            return farPlane; // Return farPlane if no object is visible
        }

        glm::vec4 ndc = glm::vec4(0.0f, 0.0f, minDepth * 2.0f - 1.0f, 1.0f);
        glm::mat4 invPV = glm::inverse(projection * view);
        glm::vec4 worldPos = invPV * ndc;
        worldPos /= worldPos.w;

        float distance = glm::distance(camera.Position, glm::vec3(worldPos));

        return distance;
    }

private:
    float lastScrollTime = 0.0f;


    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }

    float easeOutCubic(float t) {
        return 1 - pow(1 - t, 3);
    }
};

#endif