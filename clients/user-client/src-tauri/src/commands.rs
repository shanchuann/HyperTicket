use std::sync::Mutex;
use tauri::State;
use serde_json::Value;
use crate::tcp_client::TcpClient;

/// 通用请求入口：接受任意 JSON payload，返回后端响应
#[tauri::command]
pub fn send_request(
    client: State<'_, Mutex<TcpClient>>,
    payload: Value,
) -> Result<Value, String> {
    let json = serde_json::to_string(&payload).map_err(|e| e.to_string())?;
    let response = client.lock()
        .map_err(|_| "锁获取失败".to_string())?
        .send_request(&json)?;
    serde_json::from_str(&response)
        .map_err(|e| format!("响应解析失败: {}", e))
}

#[tauri::command]
pub fn check_connection(client: State<'_, Mutex<TcpClient>>) -> bool {
    client.lock().map(|mut c| c.check_connection()).unwrap_or(false)
}
