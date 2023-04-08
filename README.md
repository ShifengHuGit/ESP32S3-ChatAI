# Wake Word  : Alexa
Say [Alexa]  to wake module up, 
then module will have 5 seconds to record the voice, encode it and upload to Azure.
Azure Cognitive service will convert the wav voice to text, send it back to the module.
ESP now transfer the Text to OPENAI via its API.  and show the response returned by OPENAI as dialog.
the tricky thing is In China, GFW block the API endpoint, I built a Proxy server on CLoud. transparently transfer the API request to OPENAPI.
<br>
![Arch Image](https://github.com/ShifengHuGit/ESP32S3-ChatAI/blob/cd128a5d7292cd4af673be798f10dfe760851e50/img/Screen%20Shot%202023-03-10%20at%2013.03.52.png)
