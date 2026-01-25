package com.network;

import java.io.DataInputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.text.SimpleDateFormat;

public class Server {

    public static void main(String[] args) {
        int port = args.length > 0 ? Integer.parseInt(args[0]) : 12345;

        System.out.println("Server started on port " + port);
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy.MM.dd HH:mm:ss");

        try (ServerSocket serverSocket = new ServerSocket(port)) {

            while (true) {
                try (Socket client = serverSocket.accept()) {
                    client.setTcpNoDelay(true);
                    
                    String clientInfo = client.getInetAddress().getHostAddress() + ":" + client.getPort();
                    System.out.println("Client connected: " + clientInfo);

                    DataInputStream in = new DataInputStream(client.getInputStream());
                    OutputStream out = client.getOutputStream();

                    byte[] buffer = new byte[65536];
                    int requestCount = 0;

                    try {
                        while (true) {
                            // Читаем размер данных (4 байта)
                            int dataSize = in.readInt();
                            
                            // Читаем точно dataSize байт
                            int totalRead = 0;
                            while (totalRead < dataSize) {
                                int bytesRead = in.read(buffer, 0, Math.min(buffer.length, dataSize - totalRead));
                                if (bytesRead == -1) break;
                                totalRead += bytesRead;
                            }
                            
                            requestCount++;
                            
                            String dateResponse = dateFormat.format(new java.util.Date()) + "\n";
                            byte[] responseBytes = dateResponse.getBytes();

                            out.write(responseBytes);
                            out.flush();
                            
                            System.out.printf("Request #%d from %s: received %d bytes, sent response%n", 
                                             requestCount, clientInfo, totalRead);
                        }
                    } catch (java.io.EOFException e) {
                        // Клиент закрыл соединение - это нормально
                    }
                    
                    System.out.println("Client disconnected: " + clientInfo + " (total requests: " + requestCount + ")");
                } catch (Exception e) {
                    System.err.println("Client error: " + e.getMessage());
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
            