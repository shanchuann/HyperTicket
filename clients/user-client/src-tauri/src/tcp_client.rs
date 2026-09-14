use std::io::{BufRead, BufReader, Write};
use std::net::TcpStream;
use std::time::Duration;

pub struct TcpClient {
    host: String,
    port: u16,
    stream: Option<TcpStream>,
    reader: Option<BufReader<TcpStream>>,
}

impl TcpClient {
    pub fn new(host: String, port: u16) -> Self {
        TcpClient { host, port, stream: None, reader: None }
    }

    fn ensure_connected(&mut self) -> Result<(), String> {
        if self.stream.is_some() {
            return Ok(());
        }
        let addr = format!("{}:{}", self.host, self.port);
        let stream = TcpStream::connect(&addr)
            .map_err(|e| format!("连接服务器失败: {}", e))?;
        stream.set_read_timeout(Some(Duration::from_secs(15)))
            .map_err(|e| e.to_string())?;
        stream.set_write_timeout(Some(Duration::from_secs(10)))
            .map_err(|e| e.to_string())?;
        let reader_stream = stream.try_clone().map_err(|e| e.to_string())?;
        self.reader = Some(BufReader::new(reader_stream));
        self.stream = Some(stream);
        Ok(())
    }

    fn reset(&mut self) {
        self.stream = None;
        self.reader = None;
    }

    pub fn send_request(&mut self, json: &str) -> Result<String, String> {
        self.ensure_connected()?;

        // Send
        let msg = format!("{}\n", json);
        let write_ok = self.stream.as_mut().unwrap()
            .write_all(msg.as_bytes()).is_ok();
        if !write_ok {
            self.reset();
            return Err("发送失败，连接已断开".to_string());
        }

        // Receive
        let mut response = String::new();
        let read_ok = self.reader.as_mut().unwrap()
            .read_line(&mut response).is_ok();
        if !read_ok || response.is_empty() {
            self.reset();
            return Err("读取响应失败，连接已断开".to_string());
        }

        Ok(response.trim().to_string())
    }

    pub fn check_connection(&mut self) -> bool {
        self.ensure_connected().is_ok()
    }
}
