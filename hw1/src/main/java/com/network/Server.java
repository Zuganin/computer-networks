package com.network;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Date;

public class Server {

    public static void main(String[] args) {
        int port = args.length > 0 ? Integer.parseInt(args[0]) : 12345;

        System.out.println("Server started on port " + port);

        try (ServerSocket serverSocket = new ServerSocket(port)) {

            while (true) {
                try (Socket client = serverSocket.accept()) {

                    InputStream in = client.getInputStream();
                    OutputStream out = client.getOutputStream();

                    byte[] buffer = new byte[65536];
                    int bytesRead;

                    while ((bytesRead = in.read(buffer)) != -1) {
                        String dateResponse = new Date().toString() + "\n";
                        byte[] responseBytes = dateResponse.getBytes();

                        out.write(responseBytes);
                        out.flush();
                    }
                } catch (Exception e) {
                    System.err.println("Client disconnected or error: " + e.getMessage());
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}