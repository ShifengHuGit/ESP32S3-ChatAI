# Wake Word  : Alexa
Say [Alexa]  to wake module up, 
then module will have 5 seconds to record the voice, encode it and upload to Azure.
Azure Cognitive service will convert the wav voice to text, send it back to the module.
ESP now transfer the Text to OPENAI via its API.  and show the response returned by OPENAI as dialog.
the tricky thing is In China, GFW block the API endpoint, I built a Proxy server on CLoud. transparently transfer the API request to OPENAPI.
