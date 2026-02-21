<?php

require_once __DIR__ . '/config.php';
require_once __DIR__ . '/cors.php';
require_once __DIR__ . '/jwt.php';
require_once __DIR__ . '/auth.php';
require_once __DIR__ . '/user.php';

function handleApiRequest() {
    handleCors();
    addSecurityHeaders();
    
    $requestUri = $_SERVER['REQUEST_URI'];
    $scriptName = dirname($_SERVER['SCRIPT_NAME']);
    $path = str_replace($scriptName, '', $requestUri);
    $path = parse_url($path, PHP_URL_PATH);
    $path = trim($path, '/');
    
    $parts = explode('/', $path);
    
    if (empty($parts) || $parts[0] !== 'api') {
        jsonResponse(['error' => '无效的API路径'], 404);
    }
    
    array_shift($parts);
    
    if (empty($parts)) {
        jsonResponse([
            'name' => 'XiaoKangOS API',
            'version' => API_VERSION,
            'status' => 'running',
            'endpoints' => [
                'auth' => [
                    'register' => 'POST /api/auth/register',
                    'login' => 'POST /api/auth/login',
                    'logout' => 'POST /api/auth/logout',
                    'refresh' => 'POST /api/auth/refresh',
                    'csrf' => 'GET /api/auth/csrf'
                ],
                'user' => [
                    'profile' => 'GET /api/user/profile',
                    'update_profile' => 'PUT /api/user/profile',
                    'change_password' => 'POST /api/user/password',
                    'delete_account' => 'DELETE /api/user/delete',
                    'list_users' => 'GET /api/user/list (admin/moderator)',
                    'get_user' => 'GET /api/user/{id}'
                ]
            ]
        ]);
    }
    
    $module = array_shift($parts);
    $action = array_shift($parts) ?: '';
    
    try {
        switch ($module) {
            case 'auth':
                handleAuthRoute($action);
                break;
                
            case 'user':
                handleUserRoute($action, $parts);
                break;
                
            case 'health':
                requireMethod('GET');
                jsonResponse([
                    'status' => 'healthy',
                    'timestamp' => time(),
                    'database' => 'connected'
                ]);
                break;
                
            case 'info':
                requireMethod('GET');
                $user = getCurrentUser();
                jsonResponse([
                    'name' => 'XiaoKangOS API',
                    'version' => API_VERSION,
                    'authenticated' => $user !== null,
                    'user' => $user ? [
                        'id' => $user['sub'],
                        'username' => $user['username'],
                        'role' => $user['role']
                    ] : null
                ]);
                break;
                
            default:
                jsonResponse(['error' => '无效的API模块'], 404);
        }
    } catch (PDOException $e) {
        logError("API错误: " . $e->getMessage());
        jsonResponse(['error' => '服务器内部错误'], 500);
    } catch (Exception $e) {
        logError("API异常: " . $e->getMessage());
        jsonResponse(['error' => '服务器内部错误'], 500);
    }
}
