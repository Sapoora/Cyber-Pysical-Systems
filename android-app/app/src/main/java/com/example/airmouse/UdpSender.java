package com.example.airmouse;

import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.util.concurrent.LinkedBlockingQueue;

public class UdpSender {

    private static final int PORT = 5000;
    private static final int ACK_TIMEOUT_MS = 300;
    private static final int MAX_RETRIES = 3;

    private final String laptopIp;
    private DatagramSocket socket;
    private Thread senderThread;
    private Thread receiverThread;
    private volatile boolean running = false;

    private final LinkedBlockingQueue<String> queue = new LinkedBlockingQueue<>();

    // tracks whether last sent reliable packet got ACKed
    private volatile boolean ackReceived = false;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    public interface ConnectionListener {
        void onError(String message);
    }

    private ConnectionListener listener;

    public UdpSender(String laptopIp) {
        this.laptopIp = laptopIp;
    }

    public void setListener(ConnectionListener listener) {
        this.listener = listener;
    }

    public void start() {
        if (running) return;
        running = true;

        try {
            socket = new DatagramSocket();
            socket.setSoTimeout(ACK_TIMEOUT_MS);
        } catch (IOException e) {
            notifyError("Socket creation failed: " + e.getMessage());
            running = false;
            return;
        }

        senderThread = new Thread(this::senderLoop);
        senderThread.start();
    }

    public void stop() {
        running = false;
        queue.clear();
        if (senderThread != null) senderThread.interrupt();
        if (socket != null) socket.close();
    }

    // movement-only packets: safe to drop, no ACK needed
    public void sendMove(float deltaX, float deltaY) {
        if (!running) return;
        // keep queue small for movement packets, drop oldest if full
        queue.poll(); // drop previous unsent move packet if any
        String json = buildJson(deltaX, deltaY, false, 0);
        queue.offer(json);
    }

    // click/scroll packets: must be delivered reliably
    public void sendClick() {
        sendReliable(buildJson(0, 0, true, 0));
    }

    public void sendScroll(int direction) {
        sendReliable(buildJson(0, 0, false, direction));
    }

    private void sendReliable(String json) {
        if (!running) return;
        queue.offer("R:" + json); // prefix R: marks reliable packet
    }

    private void senderLoop() {
        try {
            InetAddress address = InetAddress.getByName(laptopIp);

            while (running) {
                String item = queue.poll();
                if (item == null) {
                    Thread.sleep(5);
                    continue;
                }

                boolean reliable = item.startsWith("R:");
                String json = reliable ? item.substring(2) : item;
                byte[] data = json.getBytes();
                DatagramPacket packet = new DatagramPacket(data, data.length, address, PORT);

                if (!reliable) {
                    socket.send(packet);
                } else {
                    sendWithAck(packet);
                }
            }
        } catch (Exception e) {
            notifyError("Sender error: " + e.getMessage());
        }
    }

    private void sendWithAck(DatagramPacket packet) {
        int attempts = 0;
        while (attempts < MAX_RETRIES && running) {
            try {
                socket.send(packet);

                byte[] buffer = new byte[64];
                DatagramPacket response = new DatagramPacket(buffer, buffer.length);
                socket.receive(response); // blocks until ACK or timeout

                String reply = new String(response.getData(), 0, response.getLength());
                if (reply.contains("ACK")) {
                    return; // success
                }
            } catch (IOException timeoutOrError) {
                attempts++;
            }
        }
        notifyError("Failed to deliver reliable packet after retries");
    }

    private String buildJson(float deltaX, float deltaY, boolean click, int scroll) {
        return "{\"DeltaX\":" + deltaX +
                ",\"DeltaY\":" + deltaY +
                ",\"Click\":" + click +
                ",\"Scroll\":" + scroll + "}";
    }

    private void notifyError(String message) {
        if (listener != null) {
            mainHandler.post(() -> listener.onError(message));
        }
    }
}