package com.network;

import java.io.InputStream;
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

                    InputStream in = client.getInputStream();
                    OutputStream out = client.getOutputStream();

                    byte[] buffer = new byte[65536];
                    int bytesRead;
                    int requestCount = 0;

                    while ((bytesRead = in.read(buffer)) > 0) {
                        requestCount++;
                        
                        String dateResponse = dateFormat.format(new java.util.Date()) + "\n";
                        byte[] responseBytes = dateResponse.getBytes();

                        out.write(responseBytes);
                        out.flush();
                        
                        System.out.printf("Request #%d from %s: received %d bytes, sent response%n", 
                                         requestCount, clientInfo, bytesRead);
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
            