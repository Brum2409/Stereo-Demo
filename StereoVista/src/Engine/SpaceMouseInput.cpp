#include "Engine/SpaceMouseInput.h"
#include <iostream>
#include <mutex>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

// Include 3DConnexion headers  
#include <navlib/navlib.h>
#include <navlib/navlib_error.h>

// Navigation model implementation using direct navlib C interface
class SpaceMouseInput::NavigationModel {
public:
    NavigationModel(SpaceMouseInput* parent) 
        : m_parent(parent),
          m_navlibHandle(0),
          m_motionActive(false),
          m_transactionActive(false) {
    }

    ~NavigationModel() {
        Shutdown();
    }

    bool Initialize(const std::string& appName) {
        // Create accessor array for navlib
        static navlib::accessor_t accessors[] = {
            // View properties
            {navlib::view_affine_k, GetCameraMatrix, SetCameraMatrix, reinterpret_cast<navlib::param_t>(this)},
            {navlib::view_fov_k, GetViewFOV, SetViewFOV, reinterpret_cast<navlib::param_t>(this)},
            {navlib::view_perspective_k, GetIsViewPerspective, nullptr, reinterpret_cast<navlib::param_t>(this)},
            {navlib::view_rotatable_k, GetIsViewRotatable, nullptr, reinterpret_cast<navlib::param_t>(this)},
            
            // Model properties
            {navlib::model_extents_k, GetModelExtents, nullptr, reinterpret_cast<navlib::param_t>(this)},
            
            // Selection properties (empty for now)
            {navlib::selection_empty_k, GetIsSelectionEmpty, nullptr, reinterpret_cast<navlib::param_t>(this)},
            
            // Coordinate system
            {navlib::coordinate_system_k, GetCoordinateSystem, nullptr, reinterpret_cast<navlib::param_t>(this)},
            {navlib::views_front_k, GetFrontView, nullptr, reinterpret_cast<navlib::param_t>(this)},
            
            // Motion and transaction handling
            {navlib::motion_k, nullptr, SetMotionFlag, reinterpret_cast<navlib::param_t>(this)},
            {navlib::transaction_k, nullptr, SetTransaction, reinterpret_cast<navlib::param_t>(this)},
            
            // Pivot properties
            {navlib::pivot_position_k, GetPivotPosition, SetPivotPosition, reinterpret_cast<navlib::param_t>(this)},
            {navlib::pivot_visible_k, GetPivotVisible, SetPivotVisible, reinterpret_cast<navlib::param_t>(this)},
            {navlib::pivot_user_k, IsUserPivot, nullptr, reinterpret_cast<navlib::param_t>(this)},
            
            // Hit testing (dummy implementations)
            {navlib::hit_lookfrom_k, nullptr, SetHitLookFrom, reinterpret_cast<navlib::param_t>(this)},
            {navlib::hit_direction_k, nullptr, SetHitDirection, reinterpret_cast<navlib::param_t>(this)},
            {navlib::hit_aperture_k, nullptr, SetHitAperture, reinterpret_cast<navlib::param_t>(this)},
            {navlib::hit_selectionOnly_k, nullptr, SetHitSelectionOnly, reinterpret_cast<navlib::param_t>(this)},
            {navlib::hit_lookat_k, GetHitLookAt, nullptr, reinterpret_cast<navlib::param_t>(this)}
        };

        // Create navlib instance
        long result = navlib::NlCreate(&m_navlibHandle, appName.c_str(), accessors, 
                                       sizeof(accessors) / sizeof(accessors[0]), nullptr);
        
        if (result != 0 || m_navlibHandle == 0) {
            return false;
        }
        
        // Check if a device is actually present
        navlib::value_t devicePresent;
        long deviceResult = navlib::NlReadValue(m_navlibHandle, navlib::device_present_k, &devicePresent);
        if (deviceResult != 0 || devicePresent.type != navlib::bool_type || devicePresent.b == 0) {
            // No device present, clean up and fail
            navlib::NlClose(m_navlibHandle);
            m_navlibHandle = 0;
            return false;
        }
        
        return true;
    }

    void Shutdown() {
        if (m_navlibHandle != 0) {
            navlib::NlClose(m_navlibHandle);
            m_navlibHandle = 0;
        }
    }

    void SetEnabled(bool enabled) {
        if (m_navlibHandle != 0) {
            navlib::value_t value;
            value.type = navlib::bool_type;
            value.b = enabled ? 1 : 0;
            navlib::NlWriteValue(m_navlibHandle, navlib::active_k, &value);
        }
    }

    // Static accessor functions for navlib
    static long __cdecl GetCameraMatrix(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetCameraMatrixImpl(value);
    }

    static long __cdecl SetCameraMatrix(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->SetCameraMatrixImpl(value);
    }

    static long __cdecl GetViewFOV(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetViewFOVImpl(value);
    }

    static long __cdecl SetViewFOV(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->SetViewFOVImpl(value);
    }

    static long __cdecl GetIsViewPerspective(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetIsViewPerspectiveImpl(value);
    }

    static long __cdecl GetIsViewRotatable(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetIsViewRotatableImpl(value);
    }

    static long __cdecl GetModelExtents(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetModelExtentsImpl(value);
    }

    static long __cdecl GetIsSelectionEmpty(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetIsSelectionEmptyImpl(value);
    }

    static long __cdecl GetCoordinateSystem(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetCoordinateSystemImpl(value);
    }

    static long __cdecl GetFrontView(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetFrontViewImpl(value);
    }

    static long __cdecl SetMotionFlag(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->SetMotionFlagImpl(value);
    }

    static long __cdecl SetTransaction(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->SetTransactionImpl(value);
    }

    static long __cdecl GetPivotPosition(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetPivotPositionImpl(value);
    }

    static long __cdecl SetPivotPosition(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->SetPivotPositionImpl(value);
    }

    static long __cdecl GetPivotVisible(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->GetPivotVisibleImpl(value);
    }

    static long __cdecl SetPivotVisible(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->SetPivotVisibleImpl(value);
    }

    static long __cdecl IsUserPivot(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        NavigationModel* self = reinterpret_cast<NavigationModel*>(param);
        return self->IsUserPivotImpl(value);
    }

    // Dummy hit testing functions
    static long __cdecl SetHitLookFrom(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) { return 0; }
    static long __cdecl SetHitDirection(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) { return 0; }
    static long __cdecl SetHitAperture(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) { return 0; }
    static long __cdecl SetHitSelectionOnly(navlib::param_t param, navlib::property_t name, const navlib::value_t* value) { return 0; }
    static long __cdecl GetHitLookAt(navlib::param_t param, navlib::property_t name, navlib::value_t* value) {
        return navlib::make_result_code(navlib::navlib_errc::no_data_available);
    }

    SpaceMouseInput* m_parent;
    navlib::nlHandle_t m_navlibHandle;
    mutable std::mutex m_mutex;
    
public:
    bool m_motionActive;
    bool m_transactionActive;
    
    void RefreshPivotPosition() {
        if (m_navlibHandle != 0 && m_parent->m_anchorMode != GUI::SPACEMOUSE_ANCHOR_DISABLED) {
            glm::vec3 pivotPoint = GetCurrentPivotPoint();
            
            // Force NavLib to re-query the pivot position by writing the pivot position value
            navlib::value_t pivotValue;
            pivotValue.type = navlib::point_type;
            pivotValue.point.x = pivotPoint.x;
            pivotValue.point.y = pivotPoint.y;
            pivotValue.point.z = pivotPoint.z;
            
            // Try to write the pivot position to force update
            navlib::NlWriteValue(m_navlibHandle, navlib::pivot_position_k, &pivotValue);
        }
    }
    
    glm::vec3 GetCurrentPivotPoint() const {
        switch (m_parent->m_anchorMode) {
            case GUI::SPACEMOUSE_ANCHOR_ON_START:
                return m_parent->m_navigationStartAnchor;
            case GUI::SPACEMOUSE_ANCHOR_CONTINUOUS:
                return m_parent->m_cursorAnchor;
            case GUI::SPACEMOUSE_ANCHOR_DISABLED:
            default:
                return (m_parent->m_modelMin + m_parent->m_modelMax) * 0.5f;
        }
    }
    
private:

    // Implementation functions
    long GetCameraMatrixImpl(navlib::value_t* value) {
        if (!m_parent->m_camera) {
            return navlib::make_result_code(navlib::navlib_errc::no_data_available);
        }

        glm::mat4 viewMatrix = m_parent->m_camera->GetViewMatrix();
        glm::mat4 cameraMatrix = glm::inverse(viewMatrix);
        
        value->type = navlib::matrix_type;
        double* matrixData = &value->matrix[0];
        m_parent->ConvertToNavlibMatrix(cameraMatrix, matrixData);
        return 0;
    }

    long SetCameraMatrixImpl(const navlib::value_t* value) {
        if (!m_parent->m_camera || value->type != navlib::matrix_type) {
            return navlib::make_result_code(navlib::navlib_errc::invalid_argument);
        }

        const double* matrixData = &value->matrix[0];
        glm::mat4 cameraMatrix = m_parent->ConvertNavlibMatrix(matrixData);
        
        // Get current camera state for deadzone comparison
        glm::vec3 currentPosition = m_parent->m_camera->Position;
        glm::vec3 currentFront = m_parent->m_camera->Front;
        glm::vec3 currentUp = m_parent->m_camera->Up;
        
        glm::vec3 newPosition = glm::vec3(cameraMatrix[3]);
        glm::vec3 newForward = -glm::normalize(glm::vec3(cameraMatrix[2]));
        glm::vec3 newUp = glm::normalize(glm::vec3(cameraMatrix[1]));

        // Apply deadzone filtering
        glm::vec3 positionDelta = newPosition - currentPosition;
        glm::vec3 forwardDelta = newForward - currentFront;
        glm::vec3 upDelta = newUp - currentUp;
        
        float positionMagnitude = glm::length(positionDelta);
        float rotationMagnitude = glm::length(forwardDelta) + glm::length(upDelta);
        
        // Only update if movement exceeds deadzone threshold
        if (positionMagnitude > m_parent->m_deadzone || rotationMagnitude > m_parent->m_deadzone * 0.1f) {
            m_parent->m_camera->Position = newPosition;
            m_parent->m_camera->Front = newForward;
            m_parent->m_camera->Up = newUp;
            m_parent->m_camera->Right = glm::normalize(glm::cross(newForward, newUp));

            m_parent->m_camera->Pitch = glm::degrees(asin(newForward.y));
            m_parent->m_camera->Yaw = glm::degrees(atan2(newForward.z, newForward.x));
        }

        return 0;
    }

    long GetViewFOVImpl(navlib::value_t* value) {
        value->type = navlib::double_type;
        value->d = glm::radians(m_parent->m_fieldOfView);
        return 0;
    }

    long SetViewFOVImpl(const navlib::value_t* value) {
        if (value->type != navlib::double_type) {
            return navlib::make_result_code(navlib::navlib_errc::invalid_argument);
        }
        m_parent->m_fieldOfView = glm::degrees(value->d);
        return 0;
    }

    long GetIsViewPerspectiveImpl(navlib::value_t* value) {
        value->type = navlib::bool_type;
        value->b = m_parent->m_perspectiveMode ? 1 : 0;
        return 0;
    }

    long GetIsViewRotatableImpl(navlib::value_t* value) {
        value->type = navlib::bool_type;
        value->b = 1; // Always allow rotation
        return 0;
    }

    long GetModelExtentsImpl(navlib::value_t* value) {
        value->type = navlib::box_type;
        value->box.min.x = m_parent->m_modelMin.x;
        value->box.min.y = m_parent->m_modelMin.y;
        value->box.min.z = m_parent->m_modelMin.z;
        value->box.max.x = m_parent->m_modelMax.x;
        value->box.max.y = m_parent->m_modelMax.y;
        value->box.max.z = m_parent->m_modelMax.z;
        return 0;
    }

    long GetIsSelectionEmptyImpl(navlib::value_t* value) {
        value->type = navlib::bool_type;
        value->b = 1; // No selection support
        return 0;
    }

    long GetCoordinateSystemImpl(navlib::value_t* value) {
        value->type = navlib::matrix_type;
        // Identity matrix - same coordinate system
        for (int i = 0; i < 16; ++i) {
            value->matrix[i] = 0.0;
        }
        value->matrix[0] = value->matrix[5] = value->matrix[10] = value->matrix[15] = 1.0;
        return 0;
    }

    long GetFrontViewImpl(navlib::value_t* value) {
        value->type = navlib::matrix_type;
        // Identity matrix - default front view
        for (int i = 0; i < 16; ++i) {
            value->matrix[i] = 0.0;
        }
        value->matrix[0] = value->matrix[5] = value->matrix[10] = value->matrix[15] = 1.0;
        return 0;
    }

    long SetMotionFlagImpl(const navlib::value_t* value) {
        if (value->type != navlib::bool_type) {
            return navlib::make_result_code(navlib::navlib_errc::invalid_argument);
        }
        
        bool motion = (value->b != 0);
        bool wasNavigating = m_motionActive;
        m_motionActive = motion;
        
        // Update parent navigation state
        m_parent->m_isNavigating = motion;
        
        if (motion && !wasNavigating) {
            // Navigation starting - capture current cursor position for ON_START mode
            if (m_parent->m_anchorMode == GUI::SPACEMOUSE_ANCHOR_ON_START) {
                m_parent->m_navigationStartAnchor = m_parent->m_cursorAnchor;
            }
            
            if (m_parent->OnNavigationStarted) {
                m_parent->OnNavigationStarted();
            }
        } else if (!motion && wasNavigating) {
            if (m_parent->OnNavigationEnded) {
                m_parent->OnNavigationEnded();
            }
        }
        return 0;
    }

    long SetTransactionImpl(const navlib::value_t* value) {
        if (value->type != navlib::long_type) {
            return navlib::make_result_code(navlib::navlib_errc::invalid_argument);
        }
        m_transactionActive = (value->l != 0);
        return 0;
    }

    long GetPivotPositionImpl(navlib::value_t* value) {
        if (!m_parent->m_camera) {
            return navlib::make_result_code(navlib::navlib_errc::no_data_available);
        }

        glm::vec3 pivotPoint = GetCurrentPivotPoint();
        
        value->type = navlib::point_type;
        value->point.x = pivotPoint.x;
        value->point.y = pivotPoint.y;
        value->point.z = pivotPoint.z;
        return 0;
    }

    long SetPivotPositionImpl(const navlib::value_t* value) {
        return 0; // Accept but ignore
    }

    long GetPivotVisibleImpl(navlib::value_t* value) {
        value->type = navlib::bool_type;
        value->b = 0; // Don't show pivot
        return 0;
    }

    long SetPivotVisibleImpl(const navlib::value_t* value) {
        return 0; // Accept but ignore
    }

    long IsUserPivotImpl(navlib::value_t* value) {
        value->type = navlib::bool_type;
        value->b = 0; // Use automatic pivot
        return 0;
    }
};

// SpaceMouseInput implementation
SpaceMouseInput::SpaceMouseInput()
    : m_enabled(false)
    , m_perspectiveMode(true)
    , m_fieldOfView(45.0f)
    , m_windowWidth(800)
    , m_windowHeight(600)
    , m_modelMin(-1.0f)
    , m_modelMax(1.0f)
    , m_translationSensitivity(1.0f)
    , m_rotationSensitivity(1.0f)
    , m_deadzone(0.025f)
    , m_isNavigating(false)
    , m_lastUpdateTime(0.0f)
    , m_cursorAnchor(0.0f)
    , m_anchorMode(GUI::SPACEMOUSE_ANCHOR_DISABLED)
    , m_centerCursor(false)
    , m_navigationStartAnchor(0.0f) {
}

SpaceMouseInput::~SpaceMouseInput() {
    Shutdown();
}

bool SpaceMouseInput::Initialize(const std::string& appName) {
    try {
        m_navigationModel = std::make_unique<NavigationModel>(this);
        
        if (!m_navigationModel->Initialize(appName)) {
            std::cerr << "Failed to initialize NavLib" << std::endl;
            return false;
        }
        
        m_navigationModel->SetEnabled(true);
        m_enabled = true;
        
        std::cout << "SpaceMouse navigation initialized successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "SpaceMouse initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void SpaceMouseInput::Shutdown() {
    if (m_navigationModel) {
        m_navigationModel->Shutdown();
        m_navigationModel.reset();
    }
    m_enabled = false;
}

void SpaceMouseInput::SetCamera(std::shared_ptr<Camera> camera) {
    m_camera = camera;
}

void SpaceMouseInput::SetModelExtents(const glm::vec3& min, const glm::vec3& max) {
    m_modelMin = min;
    m_modelMax = max;
}

void SpaceMouseInput::Update(float deltaTime) {
    if (!m_enabled || !m_navigationModel || !m_camera) {
        return;
    }
    
    m_lastUpdateTime += deltaTime;
}

void SpaceMouseInput::SetEnabled(bool enabled) {
    if (m_enabled == enabled) return;
    
    m_enabled = enabled;
    
    if (m_navigationModel) {
        m_navigationModel->SetEnabled(enabled);
    }
}

void SpaceMouseInput::SetPerspectiveMode(bool perspective) {
    m_perspectiveMode = perspective;
}

void SpaceMouseInput::SetFieldOfView(float fov) {
    m_fieldOfView = fov;
}

void SpaceMouseInput::SetSensitivity(float translationSensitivity, float rotationSensitivity) {
    m_translationSensitivity = translationSensitivity;
    m_rotationSensitivity = rotationSensitivity;
}

void SpaceMouseInput::SetDeadzone(float deadzone) {
    m_deadzone = std::max(0.0f, std::min(1.0f, deadzone));
}

void SpaceMouseInput::SetWindowSize(int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;
}

void SpaceMouseInput::SetCursorAnchor(const glm::vec3& cursorPosition, GUI::SpaceMouseAnchorMode anchorMode) {
    m_cursorAnchor = cursorPosition;
    m_anchorMode = anchorMode;
}

void SpaceMouseInput::SetAnchorMode(GUI::SpaceMouseAnchorMode mode) {
    m_anchorMode = mode;
}

void SpaceMouseInput::SetCenterCursor(bool centerCursor) {
    m_centerCursor = centerCursor;
}

void SpaceMouseInput::RefreshPivotPosition() {
    if (m_navigationModel) {
        m_navigationModel->RefreshPivotPosition();
    }
}

// Helper functions for coordinate system conversion
glm::mat4 SpaceMouseInput::ConvertNavlibMatrix(const double* navlibMatrix) const {
    return glm::mat4(
        navlibMatrix[0], navlibMatrix[1], navlibMatrix[2], navlibMatrix[3],
        navlibMatrix[4], navlibMatrix[5], navlibMatrix[6], navlibMatrix[7],
        navlibMatrix[8], navlibMatrix[9], navlibMatrix[10], navlibMatrix[11],
        navlibMatrix[12], navlibMatrix[13], navlibMatrix[14], navlibMatrix[15]
    );
}

void SpaceMouseInput::ConvertToNavlibMatrix(const glm::mat4& glmMatrix, double* navlibMatrix) const {
    const float* glmData = glm::value_ptr(glmMatrix);
    for (int i = 0; i < 16; ++i) {
        navlibMatrix[i] = static_cast<double>(glmData[i]);
    }
}

glm::vec3 SpaceMouseInput::ConvertNavlibPoint(const double* navlibPoint) const {
    return glm::vec3(navlibPoint[0], navlibPoint[1], navlibPoint[2]);
}

void SpaceMouseInput::ConvertToNavlibPoint(const glm::vec3& glmPoint, double* navlibPoint) const {
    navlibPoint[0] = glmPoint.x;
    navlibPoint[1] = glmPoint.y;
    navlibPoint[2] = glmPoint.z;
}