(() => {
  'use strict';

  const canvas = document.getElementById('game');
  const ctx = canvas.getContext('2d');
  const el = id => document.getElementById(id);
  const ui = {
    disc:el('discSelect'), plastic:el('plasticSelect'), throwStyle:el('throwStyle'), power:el('power'), aim:el('aim'), hyzer:el('hyzer'), nose:el('nose'),
    powerReadout:el('powerReadout'), aimReadout:el('aimReadout'), hyzerReadout:el('hyzerReadout'), noseReadout:el('noseReadout'),
    throwBtn:el('throwBtn'), replayBtn:el('replayBtn'), resetBtn:el('resetBtn'), startBtn:el('startBtn'), center:el('centerMessage'), modeBadge:el('shotModeBadge'),
    holeNum:el('holeNum'), par:el('par'), holeDist:el('holeDist'), strokes:el('strokes'), score:el('score'),
    carry:el('carry'), toPin:el('toPin'), speed:el('speed'), height:el('height'), spin:el('spin'), lie:el('lie'), surface:el('surface'), releaseQuality:el('releaseQuality'),
    windText:el('windText'), windArrow:el('windArrow'), gustText:el('gustText'), shotLabel:el('shotLabel'), shotDetail:el('shotDetail'),
    releaseMeter:el('releaseMeter'), sweetZone:el('sweetZone'), meterNeedle:el('meterNeedle'), releasePrompt:el('releasePrompt'), cameraLabel:el('cameraLabel'), cameraChip:el('cameraChip'), replayBug:el('replayBug'),
    discName:el('discName'), discFlight:el('discFlight'), discTrait:el('discTrait')
  };

  const discs = {
    apex:{name:'Apex',type:'Distance Driver',speed:12,glide:5,turn:-1,fade:3,mass:.175,maxMph:72,spinRpm:1080,drag:.00135,lift:.79,skip:.82,turnStart:47,color:'#eef04f'},
    vector:{name:'Vector',type:'Control Driver',speed:9,glide:5,turn:-2,fade:2,mass:.174,maxMph:68,spinRpm:1040,drag:.00141,lift:.82,skip:.71,turnStart:44,color:'#f39b49'},
    line:{name:'Line',type:'Fairway Driver',speed:7,glide:5,turn:-1,fade:2,mass:.173,maxMph:65,spinRpm:1000,drag:.00145,lift:.83,skip:.62,turnStart:42,color:'#68d9b0'},
    compass:{name:'Compass',type:'Midrange',speed:5,glide:5,turn:0,fade:1,mass:.177,maxMph:58,spinRpm:900,drag:.00159,lift:.81,skip:.40,turnStart:38,color:'#76b7f2'},
    touch:{name:'Touch',type:'Putt & Approach',speed:2,glide:3,turn:0,fade:1,mass:.175,maxMph:50,spinRpm:820,drag:.00180,lift:.73,skip:.22,turnStart:34,color:'#ec72a7'}
  };
  const plastics = {
    crystal:{name:'Crystal',stability:.15,speed:1.005,glide:1.00,skip:1.16,grip:.94,trait:'stable / slick skip'},
    tour:{name:'Tour',stability:0,speed:1.00,glide:1.015,skip:1.00,grip:1.00,trait:'balanced grip'},
    base:{name:'Base',stability:-.12,speed:.985,glide:.985,skip:.66,grip:1.06,trait:'grippy / less skip'}
  };

  const courses = [
    {name:'Pine Ridge Championship',hole:1,par:3,length:405,elevation:-8,wind:{speed:7,gust:4,angle:35},
      fairway:[[-45,0],[-35,80],[-28,170],[-42,270],[-24,360],[0,405]],basket:{x:0,y:405,z:-8},
      trees:[[-72,70,13],[-58,120,10],[60,115,11],[70,170,15],[-66,215,14],[55,245,9],[-54,315,11],[43,340,10],[28,385,7],[-35,382,7]]},
    {name:'Pine Ridge Championship',hole:2,par:4,length:735,elevation:22,wind:{speed:9,gust:5,angle:-18},
      fairway:[[-6,0],[22,120],[7,260],[-45,380],[-74,500],[-32,625],[0,735]],basket:{x:0,y:735,z:22},
      trees:[[-52,75,12],[55,85,12],[-60,145,11],[66,165,13],[-35,225,10],[45,240,9],[-82,315,16],[18,330,10],[45,410,13],[-120,450,13],[-35,515,11],[28,540,12],[-68,610,13],[52,640,13],[-40,690,9],[38,700,9]]},
    {name:'Pine Ridge Championship',hole:3,par:4,length:690,elevation:-15,wind:{speed:11,gust:7,angle:68},
      fairway:[[0,0],[0,110],[16,240],[12,390],[-10,520],[0,690]],basket:{x:0,y:690,z:-15},water:{y1:260,y2:365,x1:-95,x2:95},
      trees:[[-75,80,12],[80,95,12],[-82,165,11],[78,180,13],[-102,425,13],[94,430,13],[-68,500,12],[64,520,11],[-45,620,10],[50,635,10],[-30,672,7],[34,676,7]]}
  ];

  let holeIndex=0, course=courses[0], state='intro', strokes=0, totalToPar=0;
  let lie={x:0,y:0,z:0}, shotStart={x:0,y:0,z:0}, discBody=null, trail=[], shotSamples=[];
  let cameraMode=0, camera={x:0,y:-55}, shake=0;
  let lastTime=performance.now(), accumulator=0, meterDir=1, meterPos=.08;
  let lastRelease='—', contextualPutt=false, throwAnim=0, pendingThrow=null;
  let lastShot=null, replayBody=null, replayCursor=0, replayTrail=[], replayReturnState='aim';
  let wind={baseSpeed:7,gust:4,baseAngle:35,currentSpeed:7,currentAngle:35,time:0};
  const fixedDt=1/144, MPH_TO_FPS=1.466667;
  const cameraNames=['Broadcast','Chase','Tee','Pin'];

  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  const lerp=(a,b,t)=>a+(b-a)*t;
  const distance2=(a,b)=>Math.hypot(a.x-b.x,a.y-b.y);
  const distance3=(a,b)=>Math.hypot(a.x-b.x,a.y-b.y,a.z-b.z);
  const scoreText=v=>v===0?'E':v>0?`+${v}`:`${v}`;
  const hash=n=>{const x=Math.sin(n*12.9898+78.233)*43758.5453;return x-Math.floor(x);};

  function windVector(){
    const a=wind.currentAngle*Math.PI/180, s=wind.currentSpeed*MPH_TO_FPS;
    return{x:Math.sin(a)*s,y:Math.cos(a)*s};
  }

  function updateWind(dt){
    wind.time+=dt;
    const slow=.5+.5*Math.sin(wind.time*.53+holeIndex*1.7);
    const fast=.5+.5*Math.sin(wind.time*1.61+1.2);
    const pulse=Math.pow(Math.max(0,Math.sin(wind.time*.29+2.1)),7);
    const gustMix=.26*slow+.24*fast+.50*pulse;
    wind.currentSpeed=Math.max(1,wind.baseSpeed-wind.gust*.20+wind.gust*gustMix);
    wind.currentAngle=wind.baseAngle+Math.sin(wind.time*.41)*5+Math.sin(wind.time*.91)*2;
    const arrows=['↑','↗','→','↘','↓','↙','←','↖'];
    ui.windArrow.textContent=arrows[Math.round((((wind.currentAngle%360)+360)%360)/45)%8];
    ui.windText.textContent=`${Math.round(wind.currentSpeed)} mph`;
    const max=Math.round(wind.baseSpeed+wind.gust*.8);
    ui.gustText.textContent=wind.currentSpeed>wind.baseSpeed+1.1?`gusting · ${max} mph`:`variable · ${Math.max(1,Math.round(wind.baseSpeed-wind.gust*.2))}–${max}`;
  }

  function resize(){
    const dpr=Math.min(window.devicePixelRatio||1,2),w=window.innerWidth,h=window.innerHeight;
    canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);canvas.style.width=w+'px';canvas.style.height=h+'px';
    ctx.setTransform(dpr,0,0,dpr,0,0);
  }
  window.addEventListener('resize',resize);resize();

  function fairwayCenter(y){
    const pts=course.fairway;
    for(let i=0;i<pts.length-1;i++){
      if(y>=pts[i][1]&&y<=pts[i+1][1]){
        const t=(y-pts[i][1])/(pts[i+1][1]-pts[i][1]);return lerp(pts[i][0],pts[i+1][0],t);
      }
    }
    return pts[pts.length-1][0];
  }

  function terrainZ(x,y){
    const slope=course.elevation*clamp(y/course.length,0,1);
    return slope+Math.sin(y*.024)*1.3+Math.sin((x+y)*.037)*.6+Math.sin(x*.031-y*.015)*.35;
  }

  function getLieInfo(p=lie){
    if(p.y<8)return{name:'Tee',surface:'Tee pad',power:1,timing:1,friction:.95};
    const pin=distance2(p,course.basket);
    if(pin<33)return{name:'Circle 1',surface:'Green',power:1,timing:1.08,friction:1.04};
    if(pin<66)return{name:'Circle 2',surface:'Green',power:1,timing:1.04,friction:1.02};
    const center=fairwayCenter(clamp(p.y,0,course.length)),off=Math.abs(p.x-center);
    if(off<38)return{name:'Fairway',surface:'Short grass',power:1,timing:1,friction:1};
    if(off<62)return{name:'Light rough',surface:'Light rough',power:.97,timing:.90,friction:1.22};
    return{name:'Rough',surface:'Deep rough',power:.92,timing:.78,friction:1.55};
  }

  function getSurfaceAt(x,y){
    const center=fairwayCenter(clamp(y,0,course.length)),off=Math.abs(x-center);
    if(off<40)return{friction:1,skip:1};
    if(off<66)return{friction:1.25,skip:.70};
    return{friction:1.62,skip:.35};
  }

  function getShotMode(){return distance2(lie,course.basket)<=105?'putt':'throw';}

  function updateDiscCard(){
    const d=discs[ui.disc.value],p=plastics[ui.plastic.value];
    ui.discName.textContent=d.name;
    ui.discFlight.textContent=`${d.speed} · ${d.glide} · ${d.turn} · ${d.fade}`;
    ui.discTrait.textContent=`${d.type} · ${p.name} plastic · ${p.trait}`;
  }

  function syncUI(){
    ui.powerReadout.textContent=`${ui.power.value}%`;
    ui.aimReadout.textContent=`${Number(ui.aim.value)>0?'+':''}${ui.aim.value}°`;
    const h=Number(ui.hyzer.value);
    ui.hyzerReadout.textContent=h<0?`${Math.abs(h)}° hyzer`:h>0?`${h}° anhyzer`:'flat';
    ui.noseReadout.textContent=`${Number(ui.nose.value)>0?'+':''}${ui.nose.value}°`;
    updateDiscCard();
  }
  ['input','change'].forEach(evt=>[ui.power,ui.aim,ui.hyzer,ui.nose,ui.disc,ui.plastic].forEach(x=>x.addEventListener(evt,syncUI)));
  syncUI();

  function configureForLie(force=false){
    const wasPutt=contextualPutt;
    contextualPutt=getShotMode()==='putt';
    ui.modeBadge.textContent=contextualPutt?'PUTT':'THROW';
    ui.modeBadge.classList.toggle('putt',contextualPutt);
    if(contextualPutt && (!wasPutt||force)){
      ui.disc.value='touch';const puttDist=distance2(lie,course.basket);ui.power.value=Math.round(clamp(38+puttDist*.34,42,76));ui.hyzer.value=0;ui.nose.value=1;
      ui.aim.min=-18;ui.aim.max=18;
    }else if(!contextualPutt){ui.aim.min=-35;ui.aim.max=35;}
    const info=getLieInfo();
    const width=contextualPutt?18:16*info.timing;
    ui.sweetZone.style.left=`${50-width/2}%`;ui.sweetZone.style.width=`${width}%`;
    syncUI();
  }

  function setHole(i){
    holeIndex=i;course=courses[i];strokes=0;lie={x:0,y:0,z:terrainZ(0,0)};shotStart={...lie};discBody=null;trail=[];shotSamples=[];state='aim';lastRelease='—';cameraMode=0;
    camera={x:0,y:-55};throwAnim=0;pendingThrow=null;lastShot=null;replayBody=null;replayTrail=[];
    wind={baseSpeed:course.wind.speed,gust:course.wind.gust,baseAngle:course.wind.angle,currentSpeed:course.wind.speed,currentAngle:course.wind.angle,time:holeIndex*3.7};
    ui.holeNum.textContent=course.hole;ui.par.textContent=course.par;ui.holeDist.textContent=`${course.length}'`;ui.strokes.textContent='0';ui.score.textContent=scoreText(totalToPar);
    ui.shotLabel.textContent='ON THE TEE';ui.shotDetail.textContent=`${course.length} ft · Par ${course.par}`;ui.releaseQuality.textContent='—';
    setThrowButton('START THROW');ui.throwBtn.classList.remove('armed');ui.releaseMeter.classList.remove('active');ui.replayBtn.disabled=true;ui.replayBug.classList.remove('active');
    configureForLie(true);updateTelemetry();updateCameraLabel();
  }

  function beginGame(){ui.center.classList.add('hidden');setHole(0);}
  ui.startBtn.addEventListener('click',beginGame);

  function setThrowButton(label){
    ui.throwBtn.textContent=label+' ';const s=document.createElement('span');s.textContent='SPACE';ui.throwBtn.appendChild(s);
  }

  function startTiming(){
    if(state!=='aim')return;
    state='timing';meterPos=.06;meterDir=1;ui.releaseMeter.classList.add('active');ui.throwBtn.classList.add('armed');setThrowButton('RELEASE');
    const info=getLieInfo();
    ui.releasePrompt.textContent=contextualPutt?'HIT CENTER FOR A CLEAN PUTT':info.name==='Rough'?'ROUGH LIE · SMALLER CLEAN WINDOW':'PRESS SPACE IN THE GREEN';
  }

  function releaseTiming(){
    if(state!=='timing')return;
    const signed=meterPos-.5, error=Math.abs(signed);let label;
    if(error<=.025)label='PERFECT'; else if(error<=.075)label='GREAT'; else if(error<=.15)label='GOOD'; else label=signed<0?'EARLY':'LATE';
    lastRelease=label;ui.releaseQuality.textContent=label;ui.releaseMeter.classList.remove('active');ui.throwBtn.classList.remove('armed');setThrowButton('START THROW');
    pendingThrow={timingOffset:signed,quality:label};throwAnim=0;state='throwing';ui.shotLabel.textContent=`THROWING · ${label}`;
  }

  function handleThrowAction(){if(state==='aim')startTiming();else if(state==='timing')releaseTiming();}
  ui.throwBtn.addEventListener('click',handleThrowAction);

  function updateThrowAnimation(dt){
    if(state!=='throwing'||!pendingThrow)return;
    throwAnim+=dt/0.56;
    if(throwAnim>=.64){
      const p=pendingThrow;pendingThrow=null;launchDisc(p.timingOffset,p.quality);
    }
  }

  function launchDisc(timingOffset,quality){
    const d=discs[ui.disc.value],plastic=plastics[ui.plastic.value],lieInfo=getLieInfo();
    const power=Number(ui.power.value)/100, styleSign=ui.throwStyle.value==='rhfh'?-1:1;
    const timingAbs=Math.abs(timingOffset), timingPower=1-clamp(timingAbs*.13,0,.09);
    const aimError=timingOffset*(contextualPutt?7.5:12.5);
    const aim=(Number(ui.aim.value)+aimError)*Math.PI/180;
    const rawHyzer=Number(ui.hyzer.value)+timingOffset*(contextualPutt?3:8);
    const hyzer=rawHyzer*Math.PI/180;
    const nose=(Number(ui.nose.value)+Math.max(0,timingAbs-.07)*5)*Math.PI/180;

    let speedMph,launchDeg,spin;
    const gripBonus=plastic.grip*(quality==='PERFECT'?1.012:1);
    if(contextualPutt){
      speedMph=(18+36*power)*timingPower*lieInfo.power*gripBonus;launchDeg=10.5+Number(ui.nose.value)*.18;spin=d.spinRpm*(.35+.42*power)*plastic.grip;
    }else{
      speedMph=d.maxMph*plastic.speed*(.40+.60*power)*timingPower*lieInfo.power*gripBonus;launchDeg=(d.speed<=3?7.2:4.8)+Number(ui.nose.value)*.16;spin=d.spinRpm*(.52+.48*power)*(1-timingAbs*.08)*plastic.grip;
    }
    const speed=speedMph*MPH_TO_FPS,launch=launchDeg*Math.PI/180;
    shotStart={...lie};shotSamples=[];
    discBody={x:lie.x,y:lie.y,z:Math.max(lie.z,terrainZ(lie.x,lie.y))+(contextualPutt?4.1:4.6),
      vx:Math.sin(aim)*speed*Math.cos(launch),vy:Math.cos(aim)*speed*Math.cos(launch),vz:Math.sin(launch)*speed,
      roll:hyzer,nose,spin,speedMph,age:0,disc:d,plastic,styleSign,touchedTree:false,groundMode:'air',skipCount:0,chainContact:false,quality,color:d.color};
    trail=[{x:discBody.x,y:discBody.y,z:discBody.z}];shotSamples=[snapshotDisc(discBody)];state='flying';strokes++;ui.strokes.textContent=strokes;
    ui.shotLabel.textContent=contextualPutt?`PUTT · ${quality}`:`SHOT ${strokes} · ${quality}`;
    ui.shotDetail.textContent=`${d.name} · ${plastic.name} · ${Math.round(speedMph)} mph · ${ui.throwStyle.value==='rhfh'?'forehand':'backhand'}`;
  }

  function snapshotDisc(b){return{x:b.x,y:b.y,z:b.z,roll:b.roll,spin:b.spin,color:b.color};}

  function resetHole(){
    if(state==='flying'||state==='throwing')return;
    strokes=0;lie={x:0,y:0,z:terrainZ(0,0)};shotStart={...lie};discBody=null;trail=[];shotSamples=[];state='aim';lastRelease='—';lastShot=null;replayBody=null;replayTrail=[];
    ui.strokes.textContent='0';ui.releaseQuality.textContent='—';ui.shotLabel.textContent='ON THE TEE';ui.shotDetail.textContent=`${course.length} ft · Par ${course.par}`;
    ui.releaseMeter.classList.remove('active');ui.throwBtn.classList.remove('armed');setThrowButton('START THROW');ui.replayBtn.disabled=true;ui.replayBug.classList.remove('active');configureForLie(true);updateTelemetry();
  }
  ui.resetBtn.addEventListener('click',resetHole);

  function simulate(dt){
    if(state!=='flying'||!discBody)return;
    const b=discBody,d=b.disc,p=b.plastic;b.age+=dt;
    if(b.groundMode!=='air'){simulateGround(dt);return;}

    const w=windVector(),rvx=b.vx-w.x,rvy=b.vy-w.y,rvz=b.vz,airSpeed=Math.max(1,Math.hypot(rvx,rvy,rvz)),horiz=Math.max(1,Math.hypot(rvx,rvy));
    const flightPath=Math.atan2(rvz,horiz),aoa=b.nose-flightPath;
    const stall=Math.max(0,Math.abs(aoa)-.26);
    const dragK=d.drag*(1+.62*Math.abs(aoa)+stall*3.0);
    const drag=dragK*airSpeed*airSpeed;
    b.vx-=(rvx/airSpeed)*drag*dt;b.vy-=(rvy/airSpeed)*drag*dt;b.vz-=(rvz/airSpeed)*drag*.46*dt;

    let liftCoeff=d.lift*p.glide+aoa*2.65;
    if(aoa>.34)liftCoeff-=Math.pow(aoa-.34,2)*6.5;
    liftCoeff=clamp(liftCoeff,-.18,1.48);
    const lift=.0062*airSpeed*airSpeed*liftCoeff;
    b.vz+=lift*dt;b.vz-=32.174*dt;

    const speedMph=Math.hypot(b.vx,b.vy,b.vz)/MPH_TO_FPS;
    const high=clamp((speedMph-d.turnStart)/Math.max(8,d.maxMph-d.turnStart),0,1),low=1-high,spinFactor=clamp(b.spin/d.spinRpm,.18,1.2);
    const turnRating=Math.max(0,-d.turn-p.stability*.9),fadeRating=Math.max(.2,d.fade+p.stability*.8);
    const turnForce=turnRating*2.35*high*high*spinFactor;
    const fadeForce=fadeRating*2.55*low*low*(1.14-.30*spinFactor);
    const releaseBank=Math.sin(b.roll)*4.3*Math.exp(-b.age*.34);
    const crosswind=(w.x*(rvy/horiz)-w.y*(rvx/horiz))*.035;
    const lateralAccel=b.styleSign*(turnForce-fadeForce+releaseBank)+crosswind;
    const fx=b.vx/horiz,fy=b.vy/horiz;
    b.vx+=fy*lateralAccel*dt;b.vy-=fx*lateralAccel*dt;

    const targetRoll=b.styleSign*((high*turnRating*4.4)-(low*fadeRating*8.0))*Math.PI/180;
    b.roll+=(targetRoll-b.roll)*dt*(.30+low*.85);b.spin*=Math.pow(.993,dt*60);
    b.x+=b.vx*dt;b.y+=b.vy*dt;b.z+=b.vz*dt;
    handleTrees(b);handleBasket(b,speedMph);if(state!=='flying')return;handleGroundContact(b);if(state!=='flying')return;
    if(b.y<-100||b.y>course.length+270||Math.abs(b.x)>360||b.age>20)finishFlight(false);
    if(state==='flying'&&(trail.length===0||distance3(trail[trail.length-1],b)>1.7)){const s=snapshotDisc(b);trail.push(s);shotSamples.push(s);}
  }

  function handleTrees(b){
    for(const t of course.trees){
      const dx=b.x-t[0],dy=b.y-t[1],radius=t[2]*.30+1.25,base=terrainZ(t[0],t[1]);
      if(dx*dx+dy*dy<radius*radius&&b.z<base+t[2]*2.3&&b.z>base+.8){
        const len=Math.max(.1,Math.hypot(dx,dy)),nx=dx/len,ny=dy/len,dot=b.vx*nx+b.vy*ny;
        b.vx=(b.vx-1.72*dot*nx)*.40;b.vy=(b.vy-1.72*dot*ny)*.40;b.vz*=.34;b.spin*=.72;b.touchedTree=true;shake=.62;ui.shotLabel.textContent='TREE KICK';return;
      }
    }
  }

  function handleBasket(b,speedMph){
    const bx=course.basket.x,by=course.basket.y,bz=terrainZ(bx,by),pinDist=Math.hypot(b.x-bx,b.y-by);
    if(pinDist<2.75&&b.z>bz+1.65&&b.z<bz+5.7&&!b.chainContact){
      b.chainContact=true;const centered=pinDist<1.35,catchable=speedMph<34&&(centered||speedMph<27);
      if(catchable){b.x=bx;b.y=by;b.z=bz+3.1;b.vx=b.vy=b.vz=0;shotSamples.push(snapshotDisc(b));holeOut();return;}
      b.vx*=-.12;b.vy*=-.12;b.vz=Math.min(1.5,Math.abs(b.vz)*.12);b.spin*=.45;shake=.35;ui.shotLabel.textContent='CHAIN OUT';ui.shotDetail.textContent='Caught metal but did not stick';
    }
  }

  function handleGroundContact(b){
    const ground=terrainZ(b.x,b.y)+.32;if(b.z>ground||b.vz>=0)return;
    const inWater=course.water&&b.y>course.water.y1&&b.y<course.water.y2&&b.x>course.water.x1&&b.x<course.water.x2;
    if(inWater){b.z=ground;b.vx=b.vy=b.vz=0;shotSamples.push(snapshotDisc(b));finishFlight(true);return;}
    const surface=getSurfaceAt(b.x,b.y),groundSpeed=Math.hypot(b.vx,b.vy),impact=Math.abs(b.vz),speedMph=groundSpeed/MPH_TO_FPS,bankDeg=Math.abs(b.roll*180/Math.PI);
    const shallow=impact<13,canSkip=b.disc.skip*b.plastic.skip*surface.skip>.40&&speedMph>27&&shallow&&bankDeg<23&&b.skipCount<2;
    if(canSkip){
      const sk=b.disc.skip*b.plastic.skip*surface.skip;b.z=ground+.16;b.vz=Math.max(2.2,impact*(.13+.11*sk));b.vx*=.68+.08*sk;b.vy*=.68+.08*sk;b.spin*=.83;b.skipCount++;ui.shotLabel.textContent='SKIP';return;
    }
    if(bankDeg>28&&speedMph>14&&surface.friction<1.4){
      b.z=ground;b.vz=0;b.groundMode='roll';b.rollDirection=Math.sign(b.roll||1)*b.styleSign;b.vx*=.56;b.vy*=.56;ui.shotLabel.textContent='EDGE ROLL';return;
    }
    b.z=ground;b.vz=0;b.groundMode='slide';const damp=surface.friction>1.4?.42:.62;b.vx*=damp;b.vy*=damp;
  }

  function simulateGround(dt){
    const b=discBody;if(!b)return;const speed=Math.hypot(b.vx,b.vy);if(speed<2.0){b.vx=b.vy=0;finishFlight(false);return;}
    const surface=getSurfaceAt(b.x,b.y);
    if(b.groundMode==='roll'){
      const nx=-b.vy/speed,ny=b.vx/speed,curve=(b.rollDirection||1)*2.6;b.vx+=nx*curve*dt;b.vy+=ny*curve*dt;
      const f=Math.pow(.972/surface.friction**.18,dt*60);b.vx*=f;b.vy*=f;
    }else{
      const f=Math.pow(.952/surface.friction**.32,dt*60);b.vx*=f;b.vy*=f;
    }
    b.x+=b.vx*dt;b.y+=b.vy*dt;b.z=terrainZ(b.x,b.y)+.32;b.spin*=Math.pow(.96,dt*60);
    const inWater=course.water&&b.y>course.water.y1&&b.y<course.water.y2&&b.x>course.water.x1&&b.x<course.water.x2;
    if(inWater){shotSamples.push(snapshotDisc(b));finishFlight(true);return;}
    if(trail.length===0||distance3(trail[trail.length-1],b)>1.7){const s=snapshotDisc(b);trail.push(s);shotSamples.push(s);}
  }

  function saveLastShot(result){
    if(shotSamples.length<2)return;
    lastShot={samples:shotSamples.map(s=>({...s})),result,discName:discBody?discBody.disc.name:ui.disc.value,plastic:discBody?discBody.plastic.name:ui.plastic.value};
    ui.replayBtn.disabled=false;
  }

  function finishFlight(water){
    const b=discBody;if(!b)return;saveLastShot(water?'WATER HAZARD':'LANDING');
    if(water){
      strokes++;const safeY=course.water.y1-18;lie={x:clamp(b.x,-70,70),y:safeY,z:terrainZ(clamp(b.x,-70,70),safeY)};ui.strokes.textContent=strokes;
      ui.shotLabel.textContent='WATER HAZARD · +1';ui.shotDetail.textContent=`Drop zone · ${Math.round(distance2(lie,course.basket))} ft to pin`;
    }else{
      lie={x:b.x,y:b.y,z:terrainZ(b.x,b.y)};const info=getLieInfo(lie);ui.shotLabel.textContent=b.touchedTree?'TREE KICK':info.name.toUpperCase();ui.shotDetail.textContent=`${Math.round(distance2(lie,course.basket))} ft to pin · ${info.surface}`;
    }
    state='aim';discBody=null;trail=[];shotSamples=[];configureForLie();
    const dist=distance2(lie,course.basket);if(!contextualPutt&&dist<270&&discs[ui.disc.value].speed>9)ui.disc.value='line';if(contextualPutt)ui.disc.value='touch';syncUI();updateTelemetry();
  }

  function holeOut(){
    if(discBody)saveLastShot('MADE PUTT');
    state='holed';const result=strokes-course.par;totalToPar+=result;ui.score.textContent=scoreText(totalToPar);
    ui.shotLabel.textContent=result===-2?'EAGLE':result===-1?'BIRDIE':result===0?'PAR':result===1?'BOGEY':`${result>0?'+':''}${result}`;
    ui.shotDetail.textContent=`Hole ${course.hole} complete in ${strokes}`;
    discBody=null;trail=[];shotSamples=[];
    setTimeout(()=>{
      ui.center.classList.remove('hidden');
      if(holeIndex<courses.length-1){
        ui.center.querySelector('.kicker').textContent='HOLE COMPLETE';ui.center.querySelector('h1').textContent=`${ui.shotLabel.textContent} · ${strokes} on Par ${course.par}`;
        ui.center.querySelector('p').textContent=`Tournament total: ${scoreText(totalToPar)}. Next: Hole ${course.hole+1}.`;
        ui.startBtn.textContent='NEXT HOLE';ui.startBtn.onclick=()=>{ui.center.classList.add('hidden');setHole(holeIndex+1);};
      }else{
        ui.center.querySelector('.kicker').textContent='PILOT ROUND COMPLETE';ui.center.querySelector('h1').textContent=`3 holes · ${scoreText(totalToPar)}`;
        ui.center.querySelector('p').textContent='Pilot 0.3 complete. Test the bag/plastics, gusting wind, lie penalties, player animation and shot replay.';
        ui.startBtn.textContent='REPLAY ROUND';ui.startBtn.onclick=()=>{totalToPar=0;ui.center.classList.add('hidden');setHole(0);};
      }
    },950);
  }

  function startReplay(){
    if(!lastShot||lastShot.samples.length<2||state!=='aim')return;
    replayReturnState=state;state='replay';replayCursor=0;replayTrail=[];replayBody={...lastShot.samples[0]};ui.replayBug.classList.add('active');ui.shotLabel.textContent='SHOT REPLAY';ui.shotDetail.textContent=`${lastShot.discName} · ${lastShot.plastic} · ${lastShot.result}`;
  }
  ui.replayBtn.addEventListener('click',startReplay);

  function updateReplay(dt){
    if(state!=='replay'||!lastShot)return;
    const rate=Math.max(12,lastShot.samples.length/3.7);replayCursor+=dt*rate;
    const i=Math.floor(replayCursor),f=replayCursor-i;
    if(i>=lastShot.samples.length-1){
      state=replayReturnState;replayBody=null;replayTrail=[];ui.replayBug.classList.remove('active');
      const info=getLieInfo();ui.shotLabel.textContent=info.name==='Tee'?'ON THE TEE':info.name.toUpperCase();ui.shotDetail.textContent=`${Math.round(distance2(lie,course.basket))} ft to pin · ${info.surface}`;return;
    }
    const a=lastShot.samples[i],b=lastShot.samples[i+1];replayBody={x:lerp(a.x,b.x,f),y:lerp(a.y,b.y,f),z:lerp(a.z,b.z,f),roll:lerp(a.roll,b.roll,f),spin:lerp(a.spin,b.spin,f),color:a.color};
    replayTrail=lastShot.samples.slice(0,i+1);
  }

  function updateTelemetry(){
    const b=discBody,p=state==='flying'&&b?b:state==='replay'&&replayBody?replayBody:lie,carry=distance2(p,shotStart);
    ui.carry.textContent=`${Math.round(carry)} ft`;ui.toPin.textContent=`${Math.round(distance2(p,course.basket))} ft`;
    ui.speed.textContent=b&&state==='flying'?`${Math.round(Math.hypot(b.vx,b.vy,b.vz)/MPH_TO_FPS)} mph`:'0 mph';
    ui.height.textContent=(b&&state==='flying')||state==='replay'?`${Math.max(0,Math.round(p.z-terrainZ(p.x,p.y)))} ft`:'0 ft';ui.spin.textContent=b&&state==='flying'?`${Math.round(b.spin)} rpm`:'0 rpm';
    const info=getLieInfo();ui.lie.textContent=state==='flying'?'In flight':state==='replay'?'Replay':info.name;ui.surface.textContent=state==='flying'?'—':info.surface;ui.releaseQuality.textContent=lastRelease;
  }

  function updateTiming(dt){
    if(state!=='timing')return;const speed=contextualPutt?1.23:1.05;meterPos+=meterDir*dt*speed;
    if(meterPos>=.985){meterPos=.985;meterDir=-1;}if(meterPos<=.015){meterPos=.015;meterDir=1;}ui.meterNeedle.style.left=`calc(${(meterPos*100).toFixed(2)}% - 2px)`;
  }

  function updateCamera(){
    const focus=state==='flying'&&discBody?discBody:state==='replay'&&replayBody?replayBody:lie;let tx=0,ty=-40;
    if(cameraMode===0){
      if((state==='flying'&&discBody)||(state==='replay'&&replayBody)){
        const toPin=distance2(focus,course.basket);ty=toPin<145?course.basket.y-115:focus.y-(contextualPutt?32:76);tx=focus.x*.26;
      }else{ty=lie.y-(contextualPutt?34:68);tx=lie.x*.18;}
    }else if(cameraMode===1){ty=focus.y-(contextualPutt?24:34);tx=focus.x*.72;}
    else if(cameraMode===2){ty=-24;tx=0;}
    else{ty=course.basket.y-108;tx=course.basket.x*.2;}
    camera.y=lerp(camera.y,ty,.055);camera.x=lerp(camera.x,tx,.045);
  }

  function updateCameraLabel(){ui.cameraLabel.textContent=cameraNames[cameraMode];ui.cameraChip.textContent=`CAM · ${cameraNames[cameraMode].toUpperCase()}`;}

  function worldToScreen(x,y,z=0){
    const W=window.innerWidth,H=window.innerHeight,dy=y-camera.y,horizon=H*.25,depth=Math.max(25,dy+70),scale=Math.min(6.8,780/depth);
    return{x:W/2+(x-camera.x)*scale,y:horizon+53000/depth-z*scale*.84,scale,depth};
  }

  function drawSky(W,H){
    const g=ctx.createLinearGradient(0,0,0,H*.58);g.addColorStop(0,'#6ba9cb');g.addColorStop(.5,'#b7d9dc');g.addColorStop(1,'#e8e7cc');ctx.fillStyle=g;ctx.fillRect(0,0,W,H);
    ctx.fillStyle='rgba(255,255,255,.48)';for(let i=0;i<7;i++){const x=(i*263+90)%W,y=118+(i%3)*46;ctx.beginPath();ctx.ellipse(x,y,70,18,0,0,Math.PI*2);ctx.fill();}
    drawMountains(W,H);
  }

  function drawMountains(W,H){
    const horizon=H*.43;ctx.fillStyle='rgba(65,91,91,.45)';ctx.beginPath();ctx.moveTo(0,horizon);
    for(let x=0;x<=W;x+=70){const y=horizon-35-55*hash(x*.13+holeIndex*7)-18*Math.sin(x*.009);ctx.lineTo(x,y);}ctx.lineTo(W,horizon);ctx.closePath();ctx.fill();
    ctx.fillStyle='rgba(48,77,65,.42)';ctx.beginPath();ctx.moveTo(0,horizon+8);for(let x=0;x<=W;x+=52){const y=horizon-12-34*hash(x*.21+4);ctx.lineTo(x,y);}ctx.lineTo(W,horizon+8);ctx.closePath();ctx.fill();
  }

  function drawCourse(W,H){
    ctx.fillStyle='#3d6632';ctx.fillRect(0,H*.41,W,H*.59);
    drawGroundTexture(W,H);
    if(course.water)drawWater();

    const left=[],right=[];for(let y=Math.max(0,camera.y-20);y<=course.length+30;y+=10){const center=fairwayCenter(y),width=34+10*Math.sin(y*.012);left.push(worldToScreen(center-width,y,terrainZ(center-width,y)));right.push(worldToScreen(center+width,y,terrainZ(center+width,y)));}
    if(left.length>1){ctx.fillStyle='#668e47';ctx.beginPath();ctx.moveTo(left[0].x,left[0].y);left.slice(1).forEach(p=>ctx.lineTo(p.x,p.y));right.reverse().forEach(p=>ctx.lineTo(p.x,p.y));ctx.closePath();ctx.fill();ctx.strokeStyle='rgba(220,234,164,.25)';ctx.lineWidth=1;ctx.stroke();}
    drawMowLines();drawRoughDetails();drawCircleOne();drawTeePad();drawTournamentRopes();drawSpectators();
    const trees=course.trees.slice().sort((a,b)=>b[1]-a[1]);trees.forEach((t,i)=>drawTree(t[0],t[1],t[2],i));
    drawBasket();drawPlayer();drawLieMarker();
  }

  function drawGroundTexture(W,H){
    for(let x=-40;x<W+60;x+=34){const h=34+Math.abs((x*17)%33);ctx.fillStyle=x%68===0?'#24492b':'#2d5730';ctx.beginPath();ctx.moveTo(x,H*.43);ctx.lineTo(x+17,H*.43-h);ctx.lineTo(x+34,H*.43);ctx.closePath();ctx.fill();}
    ctx.fillStyle='rgba(255,235,180,.045)';for(let i=0;i<90;i++){const x=hash(i*8.3)*W,y=H*.45+hash(i*12.1)*H*.55,r=1+hash(i*.93)*2;ctx.beginPath();ctx.arc(x,y,r,0,Math.PI*2);ctx.fill();}
  }

  function drawWater(){
    const c=[[course.water.x1,course.water.y1],[course.water.x2,course.water.y1],[course.water.x2,course.water.y2],[course.water.x1,course.water.y2]].map(p=>worldToScreen(p[0],p[1],terrainZ(p[0],p[1])));
    ctx.fillStyle='rgba(48,119,155,.92)';ctx.beginPath();ctx.moveTo(c[0].x,c[0].y);c.slice(1).forEach(p=>ctx.lineTo(p.x,p.y));ctx.closePath();ctx.fill();ctx.strokeStyle='rgba(205,239,248,.45)';ctx.stroke();
    for(let i=0;i<5;i++){const t=(i+1)/6,a={x:lerp(c[0].x,c[3].x,t),y:lerp(c[0].y,c[3].y,t)},b={x:lerp(c[1].x,c[2].x,t),y:lerp(c[1].y,c[2].y,t)};ctx.strokeStyle='rgba(255,255,255,.12)';ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.stroke();}
  }

  function drawMowLines(){
    for(let y=40;y<course.length;y+=42){const c=fairwayCenter(y),p1=worldToScreen(c-27,y,terrainZ(c-27,y)+.03),p2=worldToScreen(c+27,y,terrainZ(c+27,y)+.03);ctx.strokeStyle='rgba(255,255,210,.075)';ctx.lineWidth=Math.max(1,p1.scale*1.8);ctx.beginPath();ctx.moveTo(p1.x,p1.y);ctx.lineTo(p2.x,p2.y);ctx.stroke();}
  }

  function drawRoughDetails(){
    for(let i=0;i<34;i++){
      const y=40+hash(i*9.2+holeIndex)*Math.max(80,course.length-70),side=hash(i*2.1)>.5?1:-1,center=fairwayCenter(y),x=center+side*(47+hash(i*5.7)*75),z=terrainZ(x,y),p=worldToScreen(x,y,z),s=p.scale;
      if(p.y<80||p.y>innerHeight+40)continue;
      if(i%7===0){ctx.fillStyle='#6a6250';ctx.beginPath();ctx.ellipse(p.x,p.y,2.8*s,1.0*s,-.2,0,Math.PI*2);ctx.fill();}
      else{ctx.strokeStyle='rgba(119,150,72,.65)';ctx.lineWidth=Math.max(1,.12*s);for(let k=-2;k<=2;k++){ctx.beginPath();ctx.moveTo(p.x,p.y);ctx.lineTo(p.x+k*.7*s,p.y-(2.2+Math.abs(k)*.25)*s);ctx.stroke();}}
    }
  }

  function drawCircleOne(){
    const z=terrainZ(course.basket.x,course.basket.y),p=worldToScreen(course.basket.x,course.basket.y,z+.03);if(p.depth<20)return;const r=33*p.scale;ctx.save();ctx.strokeStyle='rgba(245,247,230,.25)';ctx.lineWidth=1.2;ctx.setLineDash([6,6]);ctx.beginPath();ctx.ellipse(p.x,p.y,r,r*.23,0,0,Math.PI*2);ctx.stroke();ctx.restore();
  }

  function drawTeePad(){
    const p=worldToScreen(0,2,terrainZ(0,2)+.05),s=p.scale;if(p.y<60||p.y>window.innerHeight+80)return;ctx.fillStyle='#636c69';ctx.beginPath();ctx.moveTo(p.x-3.2*s,p.y);ctx.lineTo(p.x+3.2*s,p.y);ctx.lineTo(p.x+2.3*s,p.y-6*s);ctx.lineTo(p.x-2.3*s,p.y-6*s);ctx.closePath();ctx.fill();ctx.strokeStyle='rgba(255,255,255,.12)';ctx.stroke();
  }

  function drawTournamentRopes(){
    if(course.hole!==3)return;
    for(const side of[-1,1]){ctx.strokeStyle='rgba(242,242,230,.55)';ctx.lineWidth=1;ctx.beginPath();let first=true;for(let y=420;y<course.length-20;y+=18){const x=fairwayCenter(y)+side*50,p=worldToScreen(x,y,terrainZ(x,y)+2.1);if(first){ctx.moveTo(p.x,p.y);first=false;}else ctx.lineTo(p.x,p.y);}ctx.stroke();}
  }

  function drawSpectators(){
    if(course.hole!==3)return;
    for(let y=430;y<course.length-25;y+=32){const c=fairwayCenter(y);for(const side of[-1,1]){const x=c+side*(54+(y%64)*.08),p=worldToScreen(x,y,terrainZ(x,y)),s=p.scale;if(p.y<70||p.y>innerHeight+20)continue;ctx.fillStyle='rgba(238,226,197,.88)';ctx.beginPath();ctx.arc(p.x,p.y-3.6*s,1.15*s,0,Math.PI*2);ctx.fill();ctx.fillStyle=side<0?'rgba(34,52,79,.9)':'rgba(55,73,46,.9)';ctx.fillRect(p.x-1.2*s,p.y-2.5*s,2.4*s,4.6*s);}}
  }

  function drawTree(x,y,size,index){
    const z=terrainZ(x,y),p=worldToScreen(x,y,z);if(p.y<70||p.y>innerHeight+100)return;const s=Math.max(.15,p.scale),trunkH=size*1.1*s,type=hash(index*3.7+holeIndex*11)>.56?'pine':'broadleaf';
    ctx.fillStyle='rgba(0,0,0,.17)';ctx.beginPath();ctx.ellipse(p.x+2*s,p.y+1*s,size*.55*s,size*.14*s,0,0,Math.PI*2);ctx.fill();ctx.fillStyle='#5d402d';ctx.fillRect(p.x-size*.11*s,p.y-trunkH,size*.22*s,trunkH);
    if(type==='pine'){
      for(let k=0;k<3;k++){const yy=p.y-trunkH-(k*.32*size*s),w=size*(.62-k*.10)*s;ctx.fillStyle=k===0?'#1c4629':'#245234';ctx.beginPath();ctx.moveTo(p.x,yy-size*.62*s);ctx.lineTo(p.x-w,yy+size*.28*s);ctx.lineTo(p.x+w,yy+size*.28*s);ctx.closePath();ctx.fill();}
    }else{
      ctx.fillStyle='#20472a';ctx.beginPath();ctx.arc(p.x,p.y-trunkH,size*.62*s,0,Math.PI*2);ctx.fill();ctx.fillStyle='rgba(69,118,54,.94)';ctx.beginPath();ctx.arc(p.x-size*.28*s,p.y-trunkH+size*.10*s,size*.45*s,0,Math.PI*2);ctx.arc(p.x+size*.32*s,p.y-trunkH+size*.05*s,size*.48*s,0,Math.PI*2);ctx.fill();
    }
  }

  function drawBasket(){
    const z=terrainZ(course.basket.x,course.basket.y),p=worldToScreen(course.basket.x,course.basket.y,z),s=p.scale;if(p.y<40||p.y>innerHeight+80)return;
    ctx.fillStyle='rgba(0,0,0,.18)';ctx.beginPath();ctx.ellipse(p.x,p.y,3*s,.7*s,0,0,Math.PI*2);ctx.fill();ctx.strokeStyle='#e3e7de';ctx.lineWidth=Math.max(1.5,s*.22);ctx.beginPath();ctx.moveTo(p.x,p.y);ctx.lineTo(p.x,p.y-5.3*s);ctx.stroke();ctx.fillStyle='#d8de44';ctx.fillRect(p.x-2.4*s,p.y-4.7*s,4.8*s,.65*s);ctx.strokeStyle='#d7dad0';ctx.lineWidth=Math.max(1,s*.08);for(let i=-4;i<=4;i++){ctx.beginPath();ctx.moveTo(p.x+i*.44*s,p.y-4*s);ctx.lineTo(p.x+i*.3*s,p.y-1.7*s);ctx.stroke();}ctx.beginPath();ctx.ellipse(p.x,p.y-1.55*s,2.25*s,.36*s,0,0,Math.PI*2);ctx.stroke();
    ctx.fillStyle='rgba(20,29,20,.88)';ctx.fillRect(p.x+2.2*s,p.y-5.65*s,3.7*s,1.5*s);ctx.fillStyle='#eef04f';ctx.font=`${Math.max(7,1.0*s)}px system-ui`;ctx.textAlign='center';ctx.fillText(String(course.hole),p.x+4.05*s,p.y-4.55*s);
  }

  function drawPlayer(){
    if(state==='replay')return;
    const p=worldToScreen(lie.x,lie.y,terrainZ(lie.x,lie.y)),s=p.scale;if(p.y<60||p.y>innerHeight+70)return;
    let t=0;if(state==='timing')t=.15+.08*Math.sin(performance.now()*.008);else if(state==='throwing')t=clamp(throwAnim,0,1);else if(state==='flying'&&discBody&&discBody.age<.7)t=.66+discBody.age*.48;
    const side=ui.throwStyle.value==='rhfh'?-1:1,bodyX=p.x-side*1.2*s;
    ctx.save();ctx.lineCap='round';ctx.lineJoin='round';ctx.strokeStyle='rgba(10,18,15,.92)';ctx.fillStyle='rgba(232,199,168,.98)';
    const headY=p.y-6.2*s;ctx.beginPath();ctx.arc(bodyX,headY,.62*s,0,Math.PI*2);ctx.fill();
    ctx.lineWidth=Math.max(2,.38*s);ctx.beginPath();ctx.moveTo(bodyX,p.y-5.35*s);ctx.lineTo(bodyX-side*.35*s,p.y-2.7*s);ctx.stroke();
    const hipX=bodyX-side*.35*s,hipY=p.y-2.7*s;ctx.beginPath();ctx.moveTo(hipX,hipY);ctx.lineTo(hipX-side*(1.0-.35*t)*s,p.y);ctx.moveTo(hipX,hipY);ctx.lineTo(hipX+side*(.9+.25*t)*s,p.y);ctx.stroke();
    const shoulderY=p.y-4.8*s,reach=(1.0+2.3*Math.sin(clamp(t,0,1)*Math.PI))*side;
    ctx.beginPath();ctx.moveTo(bodyX,shoulderY);ctx.lineTo(bodyX+reach*s,shoulderY+(t-.35)*.8*s);ctx.lineTo(bodyX+(reach+side*.9)*s,shoulderY+(t-.25)*1.2*s);ctx.stroke();
    ctx.beginPath();ctx.moveTo(bodyX,shoulderY);ctx.lineTo(bodyX-side*(.8+.5*t)*s,shoulderY+1.1*s);ctx.stroke();
    if(state!=='flying'&&state!=='throwing'||t<.62){ctx.fillStyle=discs[ui.disc.value].color;ctx.beginPath();ctx.ellipse(bodyX+(reach+side*.9)*s,shoulderY+(t-.25)*1.2*s,1.0*s,.25*s,-.2*side,0,Math.PI*2);ctx.fill();}
    ctx.restore();
  }

  function drawLieMarker(){
    const p=worldToScreen(lie.x,lie.y,terrainZ(lie.x,lie.y)+.08),s=p.scale;ctx.fillStyle='rgba(215,240,91,.18)';ctx.beginPath();ctx.ellipse(p.x,p.y,7*s,2.2*s,0,0,Math.PI*2);ctx.fill();
  }

  function drawAimGuide(){
    if(state!=='aim'&&state!=='timing'&&state!=='throwing')return;
    const aim=Number(ui.aim.value)*Math.PI/180,start={x:lie.x,y:lie.y,z:terrainZ(lie.x,lie.y)+.2},dsc=discs[ui.disc.value],plastic=plastics[ui.plastic.value],style=ui.throwStyle.value==='rhfh'?-1:1,hy=Number(ui.hyzer.value)/30;
    ctx.save();ctx.setLineDash([8,8]);ctx.strokeStyle=state==='timing'?'rgba(255,255,255,.78)':'rgba(215,240,91,.82)';ctx.lineWidth=2;ctx.beginPath();
    const max=contextualPutt?Math.min(110,distance2(lie,course.basket)+15):175,turnMag=Math.max(0,-dsc.turn-plastic.stability*.9),fadeMag=Math.max(.2,dsc.fade+plastic.stability*.8);
    for(let d=8;d<=max;d+=5){const t=d/max,shape=style*(turnMag*8*t*t-fadeMag*7*Math.pow(t,3)+hy*11*t),x=start.x+Math.sin(aim)*d+shape,y=start.y+Math.cos(aim)*d,z=start.z+.06,p=worldToScreen(x,y,z);if(d===8)ctx.moveTo(p.x,p.y);else ctx.lineTo(p.x,p.y);}ctx.stroke();ctx.restore();
  }

  function drawTrail(){
    const points=state==='replay'?replayTrail:trail;if(points.length<2)return;ctx.strokeStyle=state==='replay'?'rgba(255,255,255,.82)':'rgba(215,240,91,.9)';ctx.lineWidth=4;ctx.shadowColor=state==='replay'?'rgba(255,255,255,.3)':'rgba(215,240,91,.45)';ctx.shadowBlur=12;ctx.beginPath();points.forEach((q,i)=>{const p=worldToScreen(q.x,q.y,q.z);if(i===0)ctx.moveTo(p.x,p.y);else ctx.lineTo(p.x,p.y);});ctx.stroke();ctx.shadowBlur=0;
  }

  function drawDisc(){
    const b=state==='replay'?replayBody:discBody;if(!b)return;const p=worldToScreen(b.x,b.y,b.z),s=Math.max(1.8,p.scale*.48);ctx.save();ctx.translate(p.x,p.y);ctx.rotate(-b.roll);ctx.fillStyle=b.color||'#f2ec45';ctx.shadowColor='rgba(0,0,0,.4)';ctx.shadowBlur=8;ctx.beginPath();ctx.ellipse(0,0,7*s,1.9*s,0,0,Math.PI*2);ctx.fill();ctx.shadowBlur=0;ctx.strokeStyle='rgba(255,255,255,.65)';ctx.lineWidth=1;ctx.stroke();ctx.restore();
  }

  function render(){
    const W=innerWidth,H=innerHeight;ctx.save();if(shake>0){ctx.translate((Math.random()-.5)*shake*12,(Math.random()-.5)*shake*7);shake*=.90;}drawSky(W,H);drawCourse(W,H);drawAimGuide();drawTrail();drawDisc();ctx.restore();
  }

  function loop(now){
    const frame=Math.min(.05,(now-lastTime)/1000);lastTime=now;accumulator+=frame;updateWind(frame);updateTiming(frame);updateThrowAnimation(frame);updateReplay(frame);
    while(accumulator>=fixedDt){simulate(fixedDt);accumulator-=fixedDt;}updateCamera();updateTelemetry();render();requestAnimationFrame(loop);
  }
  requestAnimationFrame(loop);

  window.addEventListener('keydown',e=>{
    if(e.code==='Space'){e.preventDefault();if(state==='intro')beginGame();else handleThrowAction();}
    if(state==='aim'||state==='timing'){
      if(e.key==='a'||e.key==='A')ui.aim.value=Math.max(Number(ui.aim.min),Number(ui.aim.value)-1);
      if(e.key==='d'||e.key==='D')ui.aim.value=Math.min(Number(ui.aim.max),Number(ui.aim.value)+1);
      if(e.key==='w'||e.key==='W')ui.power.value=Math.min(100,Number(ui.power.value)+2);
      if(e.key==='s'||e.key==='S')ui.power.value=Math.max(20,Number(ui.power.value)-2);
      if(e.key==='q'||e.key==='Q')ui.hyzer.value=Math.max(-30,Number(ui.hyzer.value)-2);
      if(e.key==='e'||e.key==='E')ui.hyzer.value=Math.min(30,Number(ui.hyzer.value)+2);
      if(e.key==='1')ui.disc.value='apex';if(e.key==='2')ui.disc.value='vector';if(e.key==='3')ui.disc.value='line';if(e.key==='4')ui.disc.value='compass';if(e.key==='5')ui.disc.value='touch';
      if(e.key==='f'||e.key==='F')ui.throwStyle.value=ui.throwStyle.value==='rhbh'?'rhfh':'rhbh';
    }
    if((e.key==='p'||e.key==='P')&&state==='aim')startReplay();
    if(e.key==='c'||e.key==='C'){cameraMode=(cameraMode+1)%4;updateCameraLabel();}
    if(e.key==='r'||e.key==='R')resetHole();syncUI();
  });

  updateCameraLabel();updateTelemetry();updateWind(0);updateDiscCard();
})();
