package com.example.airmouse;

import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.LinkedBlockingQueue;

public class UdpSender {

    private static final int PORT = 5000;
    private static final int ACK_TIMEOUT_MS = 300;
    private static final int MAX_RETRIES = 3;

    private final String laptopIp;

    private DatagramSocket socket;
    private InetAddress serverAddress;

    private Thread senderThread;
    private volatile boolean running = false;

    /*
     * Only reliable events (Click and Scroll)
     * are queued and sent with ACK verification.
     * Mouse movement packets are sent directly.
     */
    private final LinkedBlockingQueue<String> reliableQueue =
            new LinkedBlockingQueue<>();

    private final Handler mainHandler =
            new Handler(Looper.getMainLooper());

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

        if (running) {
            return;
        }

        try {

            serverAddress = InetAddress.getByName(laptopIp);

            socket = new DatagramSocket();
            socket.setSoTimeout(ACK_TIMEOUT_MS);

            running = true;

            senderThread =
                    new Thread(this::senderLoop,
                            "UdpReliableSender");

            senderThread.start();

        } catch (Exception e) {

            notifyError(
                    "Socket creation failed: "
                            + e.getMessage()
            );

            running = false;
        }
    }

    public void stop() {

        running = false;

        reliableQueue.clear();

        if (senderThread != null) {
            senderThread.interrupt();
        }

        if (socket != null && !socket.isClosed()) {
            socket.close();
        }
    }

    /*
     * Mouse movement packets:
     * - Sent directly via UDP
     * - No ACK required
     * - No queue buffering
     * This minimizes latency and improves responsiveness.
     */

    public void sendMove(float deltaX, float deltaY) {

        if (!running
                || socket == null
                || serverAddress == null) {
            return;
        }

        // run the actual network send off the calling thread (sensor callback runs on main thread)
        new Thread(() -> {
            try {

                String json =
                        buildJson(deltaX,
                                deltaY,
                                false,
                                0);

                byte[] data =
                        json.getBytes(StandardCharsets.UTF_8);

                DatagramPacket packet =
                        new DatagramPacket(
                                data,
                                data.length,
                                serverAddress,
                                PORT
                        );

                socket.send(packet);

            } catch (IOException ignored) {
                /*
                 * Occasional movement packet loss is acceptable
                 * because UDP is used for low-latency transmission.
                 * Subsequent movement packets will compensate.
                 */
            }
        }).start();
    }
    public void sendClick() {

        sendReliable(
                buildJson(
                        0,
                        0,
                        true,
                        0
                )
        );
    }

    public void sendScroll(int direction) {

        sendReliable(
                buildJson(
                        0,
                        0,
                        false,
                        direction
                )
        );
    }

    private void sendReliable(String json) {

        if (!running) {
            return;
        }

        reliableQueue.offer(json);
    }

    /*
     * only Click & Scroll
     */
    private void senderLoop() {

        while (running
                && !Thread.currentThread().isInterrupted()) {

            try {

                String json =
                        reliableQueue.take();

                byte[] data =
                        json.getBytes(
                                StandardCharsets.UTF_8
                        );

                DatagramPacket packet =
                        new DatagramPacket(
                                data,
                                data.length,
                                serverAddress,
                                PORT
                        );

                sendWithAck(packet);

            } catch (InterruptedException e) {

                Thread.currentThread().interrupt();
                break;

            } catch (Exception e) {

                if (running) {

                    notifyError(
                            "Sender error: "
                                    + e.getMessage()
                    );
                }
            }
        }
    }

    private void sendWithAck(
            DatagramPacket packet) {

        int attempts = 0;

        while (attempts < MAX_RETRIES
                && running
                && !Thread.currentThread().isInterrupted()) {

            try {

                socket.send(packet);

                byte[] buffer = new byte[128];

                DatagramPacket response =
                        new DatagramPacket(
                                buffer,
                                buffer.length
                        );

                socket.receive(response);

                String reply =
                        new String(
                                response.getData(),
                                0,
                                response.getLength(),
                                StandardCharsets.UTF_8
                        );

                if (reply.contains("ACK")) {
                    return;
                }

            } catch (IOException e) {

                attempts++;
            }
        }

        if (attempts >= MAX_RETRIES
                && running) {

            notifyError(
                    "Failed to deliver reliable packet after retries"
            );
        }
    }

    private String buildJson(
            float deltaX,
            float deltaY,
            boolean click,
            int scroll) {

        return String.format(
                Locale.US,
                "{\"DeltaX\":%.2f,\"DeltaY\":%.2f,\"Click\":%b,\"Scroll\":%d}",
                deltaX,
                deltaY,
                click,
                scroll
        );
    }

    private void notifyError(
            String message) {

        if (listener == null) {
            return;
        }

        mainHandler.post(() -> {

            if (listener != null) {
                listener.onError(message);
            }
        });
    }
}