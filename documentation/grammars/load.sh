rm /data1/ben/tts/image/*
ls -r /tts/images/ | xargs -I{} bash vis.sh /tts/images/{}
