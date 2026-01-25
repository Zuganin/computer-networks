package com.network;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Random;

public class Client {

    public static void main(String[] args) {
        if (args.length < 5) {
            System.out.println("Usage: java Client <IP> <port> <N> <M> <Q>");
            return;
        }

        String ip = args[0];
        int port = Integer.parseInt(args[1]);
        int N = Integer.parseInt(args[2]);
        int M = Integer.parseInt(args[3]);
        int Q = Integer.parseInt(args[4]);

        System.out.println("Starting Client...");
        System.out.println("Connecting to " + ip + ":" + port);

        try (Socket socket = new Socket(ip, port)) {
            socket.setTcpNoDelay(true);

            OutputStream out = socket.getOutputStream();
            InputStream in = socket.getInputStream();

            byte[] recvBuffer = new byte[1024];
            Random random = new Random();

            System.out.println("Bytes;TimeMs");

            for (int k = 0; k < M; k++) {
                int size = N * k + 8;
                byte[] data = new byte[size];
                long totalTime = 0;

                for (int q = 0; q < Q; q++) {
                    random.nextBytes(data);
                    long start = System.currentTimeMillis();
                    out.write(data);
                    out.flush();
                    int read = in.read(recvBuffer);
                    long end = System.currentTimeMillis();
                    totalTime += (end - start);
                }

                double averageTime = (double) totalTime / Q;
                System.out.printf("%d;%.4f%n", size, averageTime);
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}