mod tcp_client;
mod commands;

use tcp_client::TcpClient;
use std::sync::Mutex;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(Mutex::new(TcpClient::new("127.0.0.1".to_string(), 7000)))
        .invoke_handler(tauri::generate_handler![
            commands::send_request,
            commands::check_connection,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
