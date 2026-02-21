<?php

require_once __DIR__ . '/config.php';
require_once __DIR__ . '/jwt.php';

function handleGetProfile() {
    requireMethod('GET');
    
    $user = requireAuth();
    $userId = $user['sub'];
    
    $pdo = getDb();
    
    $stmt = $pdo->prepare("
        SELECT id, username, email, display_name, avatar_url, role, created_at, last_login, is_active
        FROM users WHERE id = ?
    ");
    $stmt->execute([$userId]);
    $userData = $stmt->fetch();
    
    if (!$userData) {
        jsonResponse(['error' => '用户不存在'], 404);
    }
    
    jsonResponse([
        'user' => [
            'id' => $userData['id'],
            'username' => $userData['username'],
            'email' => $userData['email'],
            'display_name' => $userData['display_name'],
            'avatar_url' => $userData['avatar_url'],
            'role' => $userData['role'],
            'created_at' => $userData['created_at'],
            'last_login' => $userData['last_login'],
            'is_active' => (bool)$userData['is_active']
        ]
    ]);
}

function handleUpdateProfile() {
    requireMethod('PUT');
    
    $user = requireAuth();
    $userId = $user['sub'];
    
    $rawInput = file_get_contents('php://input');
    $data = json_decode($rawInput, true);
    
    if (!$data) {
        $data = $_POST;
    }
    
    $displayName = sanitizeInput($data['display_name'] ?? '');
    $avatarUrl = sanitizeInput($data['avatar_url'] ?? '');
    $currentPassword = $data['current_password'] ?? '';
    $newPassword = $data['new_password'] ?? '';
    $csrfToken = $data['csrf_token'] ?? '';
    
    $pdo = getDb();
    
    $stmt = $pdo->prepare("SELECT password_hash FROM users WHERE id = ?");
    $stmt->execute([$userId]);
    $userData = $stmt->fetch();
    
    if (!password_verify($currentPassword, $userData['password_hash'])) {
        jsonResponse(['error' => '当前密码不正确'], 400);
    }
    
    $updateFields = [];
    $updateValues = [];
    
    if (!empty($displayName)) {
        $updateFields[] = 'display_name = ?';
        $updateValues[] = $displayName;
    }
    
    if (!empty($avatarUrl)) {
        $updateFields[] = 'avatar_url = ?';
        $updateValues[] = $avatarUrl;
    }
    
    if (!empty($newPassword)) {
        if (!validatePassword($newPassword)) {
            jsonResponse(['error' => '新密码强度不足'], 400);
        }
        
        $newPasswordHash = password_hash($newPassword, PASSWORD_BCRYPT, ['cost' => 12]);
        $updateFields[] = 'password_hash = ?';
        $updateValues[] = $newPasswordHash;
    }
    
    if (!empty($updateFields)) {
        $updateFields[] = 'updated_at = datetime("now")';
        $updateValues[] = $userId;
        
        $sql = "UPDATE users SET " . implode(', ', $updateFields) . " WHERE id = ?";
        $stmt = $pdo->prepare($sql);
        $stmt->execute($updateValues);
    }
    
    logError("用户资料更新", ['user_id' => $userId]);
    
    jsonResponse(['message' => '资料更新成功']);
}

function handleChangePassword() {
    requireMethod('POST');
    
    $user = requireAuth();
    $userId = $user['sub'];
    
    $rawInput = file_get_contents('php://input');
    $data = json_decode($rawInput, true);
    
    if (!$data) {
        $data = $_POST;
    }
    
    $currentPassword = $data['current_password'] ?? '';
    $newPassword = $data['new_password'] ?? '';
    $csrfToken = $data['csrf_token'] ?? '';
    
    if (empty($currentPassword) || empty($newPassword)) {
        jsonResponse(['error' => '当前密码和新密码为必填项'], 400);
    }
    
    if (!validatePassword($newPassword)) {
        jsonResponse(['error' => '新密码强度不足'], 400);
    }
    
    $pdo = getDb();
    
    $stmt = $pdo->prepare("SELECT password_hash FROM users WHERE id = ?");
    $stmt->execute([$userId]);
    $userData = $stmt->fetch();
    
    if (!password_verify($currentPassword, $userData['password_hash'])) {
        logError("修改密码失败 - 密码错误", ['user_id' => $userId]);
        jsonResponse(['error' => '当前密码不正确'], 400);
    }
    
    $newPasswordHash = password_hash($newPassword, PASSWORD_BCRYPT, ['cost' => 12]);
    
    $updateStmt = $pdo->prepare("UPDATE users SET password_hash = ?, updated_at = datetime('now') WHERE id = ?");
    $updateStmt->execute([$newPasswordHash, $userId]);
    
    $tokenHash = hash('sha256', getBearerToken());
    $sessionsStmt = $pdo->prepare("DELETE FROM sessions WHERE user_id = ? AND token_hash != ?");
    $sessionsStmt->execute([$userId, $tokenHash]);
    
    logError("用户密码修改", ['user_id' => $userId]);
    
    jsonResponse(['message' => '密码修改成功']);
}

function handleDeleteAccount() {
    requireMethod('DELETE');
    
    $user = requireAuth();
    $userId = $user['sub'];
    
    $rawInput = file_get_contents('php://input');
    $data = json_decode($rawInput, true);
    
    if (!$data) {
        $data = $_POST;
    }
    
    $password = $data['password'] ?? '';
    $csrfToken = $data['csrf_token'] ?? '';
    
    if (empty($password)) {
        jsonResponse(['error' => '密码为必填项'], 400);
    }
    
    $pdo = getDb();
    
    $stmt = $pdo->prepare("SELECT password_hash FROM users WHERE id = ?");
    $stmt->execute([$userId]);
    $userData = $stmt->fetch();
    
    if (!password_verify($password, $userData['password_hash'])) {
        jsonResponse(['error' => '密码不正确'], 400);
    }
    
    $deleteStmt = $pdo->prepare("DELETE FROM users WHERE id = ?");
    $deleteStmt->execute([$userId]);
    
    logError("用户账户删除", ['user_id' => $userId]);
    
    jsonResponse(['message' => '账户已删除']);
}

function handleGetUserById($userId) {
    requireMethod('GET');
    
    requireAuth();
    
    $pdo = getDb();
    
    $stmt = $pdo->prepare("
        SELECT id, username, display_name, avatar_url, role, created_at
        FROM users WHERE id = ? AND is_active = 1
    ");
    $stmt->execute([$userId]);
    $userData = $stmt->fetch();
    
    if (!$userData) {
        jsonResponse(['error' => '用户不存在'], 404);
    }
    
    jsonResponse(['user' => $userData]);
}

function handleListUsers() {
    requireMethod('GET');
    
    requireRole('moderator');
    
    $page = isset($_GET['page']) ? max(1, (int)$_GET['page']) : 1;
    $limit = isset($_GET['limit']) ? min(100, max(1, (int)$_GET['limit'])) : 20;
    $offset = ($page - 1) * $limit;
    
    $pdo = getDb();
    
    $countStmt = $pdo->query("SELECT COUNT(*) as total FROM users");
    $total = $countStmt->fetch()['total'];
    
    $stmt = $pdo->prepare("
        SELECT id, username, email, display_name, avatar_url, role, created_at, last_login, is_active
        FROM users ORDER BY created_at DESC LIMIT ? OFFSET ?
    ");
    $stmt->execute([$limit, $offset]);
    $users = $stmt->fetchAll();
    
    jsonResponse([
        'users' => $users,
        'pagination' => [
            'page' => $page,
            'limit' => $limit,
            'total' => (int)$total,
            'total_pages' => ceil($total / $limit)
        ]
    ]);
}

function handleUpdateUser($userId) {
    requireMethod('PUT');
    
    requireRole('admin');
    
    $rawInput = file_get_contents('php://input');
    $data = json_decode($rawInput, true);
    
    if (!$data) {
        jsonResponse(['error' => '无效的请求数据'], 400);
    }
    
    $pdo = getDb();
    
    $stmt = $pdo->prepare("SELECT id FROM users WHERE id = ?");
    $stmt->execute([$userId]);
    if (!$stmt->fetch()) {
        jsonResponse(['error' => '用户不存在'], 404);
    }
    
    $updateFields = [];
    $updateValues = [];
    
    if (isset($data['display_name'])) {
        $updateFields[] = 'display_name = ?';
        $updateValues[] = sanitizeInput($data['display_name']);
    }
    
    if (isset($data['role'])) {
        $allowedRoles = ['user', 'moderator', 'admin'];
        if (!in_array($data['role'], $allowedRoles)) {
            jsonResponse(['error' => '无效的角色'], 400);
        }
        $updateFields[] = 'role = ?';
        $updateValues[] = $data['role'];
    }
    
    if (isset($data['is_active'])) {
        $updateFields[] = 'is_active = ?';
        $updateValues[] = $data['is_active'] ? 1 : 0;
    }
    
    if (empty($updateFields)) {
        jsonResponse(['error' => '没有要更新的字段'], 400);
    }
    
    $updateFields[] = 'updated_at = datetime("now")';
    $updateValues[] = $userId;
    
    $sql = "UPDATE users SET " . implode(', ', $updateFields) . " WHERE id = ?";
    $stmt = $pdo->prepare($sql);
    $stmt->execute($updateValues);
    
    logError("管理员更新用户", ['target_user_id' => $userId, 'admin_id' => getCurrentUser()['sub']]);
    
    jsonResponse(['message' => '用户更新成功']);
}

function handleUserRoute($action, $params = []) {
    if (empty($action)) {
        handleGetProfile();
        return;
    }
    
    if (is_numeric($action)) {
        handleGetUserById($action);
        return;
    }
    
    switch ($action) {
        case 'profile':
            if ($_SERVER['REQUEST_METHOD'] === 'GET') {
                handleGetProfile();
            } elseif ($_SERVER['REQUEST_METHOD'] === 'PUT') {
                handleUpdateProfile();
            }
            break;
            
        case 'password':
            handleChangePassword();
            break;
            
        case 'delete':
            handleDeleteAccount();
            break;
            
        case 'list':
            handleListUsers();
            break;
            
        default:
            if (is_numeric($action)) {
                $method = $_SERVER['REQUEST_METHOD'];
                if ($method === 'PUT') {
                    handleUpdateUser($action);
                } else {
                    handleGetUserById($action);
                }
            } else {
                jsonResponse(['error' => '无效的用户端点'], 404);
            }
    }
}
