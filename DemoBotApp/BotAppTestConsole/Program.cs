﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Media;
using System.Net.Http;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using DemoBotApp;

namespace BotAppTestConsole
{
    class Program
    {
        private static List<string> chatList = new List<string>()
        {
            "Hello",
            "Could you introduce yourself?",
            "Do you know microsoft?",
            "Who is Arthur?",
            "Come some music please"
        };

        static void Main(string[] args)
        {
            //TestSpeechSynthesis();
            TestWebSocket().Wait();
            //DevKitDemoAppTest().Wait();
        }

        private static async Task TestWebSocket()
        {
            int chatIndex = 2;

            using (ClientWebSocket webSocketClient = new ClientWebSocket())
            {
                string nickName = Guid.NewGuid().ToString();
                Uri serverUri = new Uri($"ws://demobotapp-sandbox.azurewebsites.net/chat?nickName={nickName}");
                await webSocketClient.ConnectAsync(serverUri, CancellationToken.None);

                List<byte> totalReceived = new List<byte>();
                ArraySegment<byte> receivedBuffer = new ArraySegment<byte>(new byte[1024 * 10]);
                WebSocketReceiveResult receiveResult;

                while (webSocketClient.State == WebSocketState.Open)
                {
                    // Send text message to server
                    string sendMsg = chatList[chatIndex];
                    chatIndex = (chatIndex + 1) % chatList.Count;

                    ArraySegment<byte> bytesToSend = new ArraySegment<byte>(Encoding.UTF8.GetBytes(sendMsg));
                    await webSocketClient.SendAsync(bytesToSend, WebSocketMessageType.Text, true, CancellationToken.None);

                    // Receive message from server
                    // receive connect ack message
                    receiveResult = await webSocketClient.ReceiveAsync(receivedBuffer, CancellationToken.None);
                    Console.WriteLine(Encoding.UTF8.GetString(receivedBuffer.Array, 0, receiveResult.Count));

                    // receive binary
                    totalReceived.Clear();
                    receiveResult = await webSocketClient.ReceiveAsync(receivedBuffer, CancellationToken.None);
                    MergeFrameContent(totalReceived, receivedBuffer.Array, receiveResult.Count);

                    try
                    {
                        while (webSocketClient.State == WebSocketState.Open && !receiveResult.EndOfMessage)
                        {
                            receiveResult = await webSocketClient.ReceiveAsync(receivedBuffer, CancellationToken.None);

                            MergeFrameContent(totalReceived, receivedBuffer.Array, receiveResult.Count);
                            Console.WriteLine($"Received: {receiveResult.Count}, total: {totalReceived.Count}");
                        }

                        //BytesToFile(totalReceived.ToArray(), @"c:\IoT\Voice\ws-routput.wav");
                        using (MemoryStream ms = new MemoryStream(totalReceived.ToArray()))
                        {
                            SoundPlayer player = new SoundPlayer(ms);
                            player.Play();
                        }
                    }
                    catch (Exception e)
                    {
                        Console.WriteLine(e.ToString());
                    }

                    Thread.Sleep(6000);
                }
            }
        }

        private static void MergeFrameContent(List<Byte> destBuffer, byte[] buffer, long count)
        {
            count = count < buffer.Length ? count : buffer.Length;

            if (count == buffer.Length)
            {
                destBuffer.AddRange(buffer);
            }
            else
            {
                var frameBuffer = new byte[count];
                Array.Copy(buffer, frameBuffer, count);

                destBuffer.AddRange(frameBuffer);
            }
        }

        private static void TestSpeechSynthesis()
        {
            foreach (string text in chatList)
            {
                Uri SpeechSynthesisUrl = new Uri("https://speech.platform.bing.com/synthesize");
                string CognitiveSubscriptionKey = "16a433c4b68241dbb136447a324be771";

                SynthesisOptions synthesisOption = new SynthesisOptions(SpeechSynthesisUrl, CognitiveSubscriptionKey);
                var ttsClient = new TTSClient(synthesisOption);
                var bytes = ttsClient.SynthesizeText(text);

                using (MemoryStream ms = new MemoryStream(bytes))
                {
                    SoundPlayer player = new SoundPlayer(ms);
                    player.Play();
                }

                Thread.Sleep(5000);
            }           
        }

        private static async Task DevKitDemoAppTest()
        {
            using (var httpclient = new HttpClient())
            {
                string wavFilePath = @"C:\\IoT\\Voice\\TTSResult-3.wav";
                using (var binaryContent = new StreamContent(File.OpenRead(@"C:\\IoT\\Voice\\STTTest-2.wav")))
                {
                    //var response = await httpclient.PostAsync("http://devkitdemobotapp.azurewebsites.net/conversation/JbgF8Sr8VxI2w4sGWM0iab", binaryContent);
                    var response = await httpclient.PostAsync("http://devkitdemobotapp-eas.azurewebsites.net/conversation/test", null);

                    var stream = await response.Content.ReadAsStreamAsync();
                    /*
                    using (FileStream fs = new FileStream(wavFilePath, FileMode.Create))
                    {
                        stream.CopyTo(fs);
                        stream.Position = 0;
                    }

                    var int16Data = new List<Int16>();
                    using (var reader = new BinaryReader(new FileStream(wavFilePath, FileMode.Open)))
                    {
                        try
                        {
                            while (true)
                            {
                                int16Data.Add(reader.ReadInt16());
                            }
                        }
                        catch
                        {
                        }
                    }

                    var int16Array = int16Data.ToArray();
                    using (var sw = new StreamWriter(@"C:\IoT\Voice\TTSResult-3.txt"))
                    {
                        for (int i = 0; i < int16Array.Length; i++)
                        {
                            if (i % 40 == 0)
                            {
                                sw.WriteLine();
                            }
                            sw.Write($"{int16Array[i].ToString()},");
                        }
                    }
                    */
                    
                    SoundPlayer player = new SoundPlayer(stream);
                    player.PlaySync();

                    stream.Dispose();
                }
            }
        }

        private static byte[] StreamToBytes(Stream input)
        {
            using (MemoryStream ms = new MemoryStream())
            {
                input.CopyTo(ms);
                byte[] bytes = ms.ToArray();
                return bytes;
            }
        }

        private static void BytesToFile(byte[] bytes, string filePath)
        {
            using (var fs = new FileStream(filePath, FileMode.Create, FileAccess.Write))
            {
                fs.Write(bytes, 0, bytes.Length);
            }
        }
    }
}