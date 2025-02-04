// ==============================================================
//	This file is part of Glest Shared Library (www.glest.org)
//
//	Copyright (C) 2001-2008 Martiño Figueroa
//
//	You can redistribute this code and/or modify it under 
//	the terms of the GNU General Public License as published 
//	by the Free Software Foundation; either version 2 of the 
//	License, or (at your option) any later version
// ==============================================================

#include "math_wrapper.h"
#include "particle.h"

#include <stdexcept>
#include <cassert>
#include <algorithm>

#include "util.h"
#include "particle_renderer.h"
#include "math_util.h"
#include "platform_common.h"
#include "conversion.h"
#include "model.h"
#include "texture.h"
#include "platform_util.h"
#include "leak_dumper.h"

using namespace std;
using namespace Shared::Util;
using namespace Shared::PlatformCommon;

namespace Shared {
namespace Graphics {

// =====================================================
//	class ParticleSystem
// =====================================================

const bool checkMemory = false;
static map<void *,int> memoryObjectList;

void Particle::saveGame(XmlNode *rootNode) {
	std::map<string,string> mapTagReplacements;
	XmlNode *particleNode = rootNode->addChild("Particle");

//	Vec3f pos;
	particleNode->addAttribute("pos",pos.getString(), mapTagReplacements);
//	Vec3f lastPos;
	particleNode->addAttribute("lastPos",lastPos.getString(), mapTagReplacements);
//	Vec3f speed;
	particleNode->addAttribute("speed",speed.getString(), mapTagReplacements);
//	Vec3f speedUpRelative;
	particleNode->addAttribute("speedUpRelative",floatToStr(speedUpRelative,6), mapTagReplacements);
//	Vec3f speedUpConstant;
	particleNode->addAttribute("speedUpConstant",speedUpConstant.getString(), mapTagReplacements);
//	Vec3f accel;
	particleNode->addAttribute("accel",accel.getString(), mapTagReplacements);
//	Vec4f color;
	particleNode->addAttribute("color",color.getString(), mapTagReplacements);
//	float size;
	particleNode->addAttribute("size",floatToStr(size,6), mapTagReplacements);
//	int energy;
	particleNode->addAttribute("energy",intToStr(energy), mapTagReplacements);
}

void Particle::loadGame(const XmlNode *rootNode) {
	const XmlNode *particleNode = rootNode;

	//particleNode = aiNode->getAttribute("startLoc")->getIntValue();
	//	Vec3f pos;
	pos = Vec3f::strToVec3(particleNode->getAttribute("pos")->getValue());
	//	Vec3f lastPos;
	lastPos = Vec3f::strToVec3(particleNode->getAttribute("lastPos")->getValue());
	//	Vec3f speed;
	speed = Vec3f::strToVec3(particleNode->getAttribute("speed")->getValue());
	//	Vec3f speed;
	speedUpRelative = particleNode->getAttribute("speedUpRelative")->getFloatValue();
	//	Vec3f speed;
	speedUpConstant = Vec3f::strToVec3(particleNode->getAttribute("speedUpConstant")->getValue());
	//	Vec3f accel;
	accel = Vec3f::strToVec3(particleNode->getAttribute("accel")->getValue());
	//	Vec4f color;
	color = Vec4f::strToVec4(particleNode->getAttribute("color")->getValue());
	//	float size;
	size = particleNode->getAttribute("size")->getFloatValue();
	//	int energy;
	energy = particleNode->getAttribute("energy")->getIntValue();
}

ParticleSystem::ParticleSystem(int particleCount) {
	if(checkMemory) {
		printf("++ Create ParticleSystem [%p]\n",this);
		memoryObjectList[this]++;
	}

	textureFileLoadDeferred = "";
	textureFileLoadDeferredSystemId = 0;
	textureFileLoadDeferredFormat = Texture::fAuto;
	textureFileLoadDeferredComponents = 0;

	//init particle vector
	blendMode= bmOne;
	//particles= new Particle[particleCount];
	particles.clear();
	//particles.reserve(particleCount);
	particles.resize(particleCount);

	state= sPlay;
	aliveParticleCount= 0;
	active= true;
	visible= true;

	//vars
	texture= NULL;
	particleObserver= NULL;

	//params
	this->particleCount= particleCount;
	//this->particleCount= particles.size();
	maxParticleEnergy= 250;
	varParticleEnergy= 50;
	pos= Vec3f(0.0f);
	color= Vec4f(1.0f);
	colorNoEnergy= Vec4f(0.0f);
	emissionRate= 15.0f;
	emissionState= 1.0f; // initialized with 1 because we must have at least one particle in the beginning!
	speed= 1.0f;
	speedUpRelative = 0;
	speedUpConstant = 0;

	teamcolorNoEnergy= false;
	teamcolorEnergy= false;
	alternations= 0;
	particleSystemStartDelay= 0;

	this->particleOwner = NULL;
	this->particleSize = 0.0f;
}

ParticleSystem::~ParticleSystem() {
	if(checkMemory) {
		printf("-- Delete ParticleSystem [%p]\n",this);
		memoryObjectList[this]--;
		assert(memoryObjectList[this] == 0);
	}

	//delete [] particles;
	particles.clear();

	delete particleObserver;
	particleObserver = NULL;
}

void ParticleSystem::callParticleOwnerEnd(ParticleSystem *particleSystem) {
	if(this->particleOwner != NULL) {
		this->particleOwner->end(particleSystem);
	}
}
Checksum ParticleSystem::getCRC() {
	Checksum crcForParticleSystem;

	//std::vector<Particle> particles;

	crcForParticleSystem.addInt(random.getLastNumber());
	crcForParticleSystem.addInt(blendMode);
	crcForParticleSystem.addInt(state);
	crcForParticleSystem.addInt(active);
	//crcForParticleSystem.addInt(visible);
	crcForParticleSystem.addInt(aliveParticleCount);
	crcForParticleSystem.addInt(particleCount);

	//string textureFileLoadDeferred;
	//int textureFileLoadDeferredSystemId;
	//Texture::Format textureFileLoadDeferredFormat;
	//int textureFileLoadDeferredComponents;

	//Texture *texture;
	//Vec3f pos;
	//Vec4f color;
	//Vec4f colorNoEnergy;
	//float emissionRate;
	//float emissionState;
	crcForParticleSystem.addInt(maxParticleEnergy);
	crcForParticleSystem.addInt(varParticleEnergy);
	//float particleSize;
	//float speed;
	//Vec3f factionColor;
	crcForParticleSystem.addInt(teamcolorNoEnergy);
	crcForParticleSystem.addInt(teamcolorEnergy);
	crcForParticleSystem.addInt(alternations);
	crcForParticleSystem.addInt(particleSystemStartDelay);
	//ParticleObserver *particleObserver;

	return crcForParticleSystem;
}

// =============== VIRTUAL ======================

//updates all living particles and creates new ones
void ParticleSystem::update() {
	if(aliveParticleCount > (int) particles.size()) {
		throw megaglest_runtime_error("aliveParticleCount >= particles.size()");
	}
    if(particleSystemStartDelay > 0) {
    	particleSystemStartDelay--;
    }
    else if(state != sPause) {
		for(int i= 0; i < aliveParticleCount; ++i) {
			updateParticle(&particles[i]);

			if(deathTest(&particles[i])) {

				//kill the particle
				killParticle(&particles[i]);

				//maintain alive particles at front of the array
				if(aliveParticleCount > 0) {
					particles[i]= particles[aliveParticleCount];
				}
			}
		}

		if(state != ParticleSystem::sFade) {
			emissionState= emissionState + emissionRate;
			int emissionIntValue= (int) emissionState;
			for(int i= 0; i < emissionIntValue; i++){
				Particle *p= createParticle();
				initParticle(p, i);
			}
			emissionState = emissionState - (float) emissionIntValue;
			emissionState = truncateDecimal<float>(emissionState,6);
		}
	}
}

void ParticleSystem::render(ParticleRenderer *pr, ModelRenderer *mr){
	if(active) {
		pr->renderSystem(this);
	}
}

ParticleSystem::BlendMode ParticleSystem::strToBlendMode(const string &str){
	if(str == "normal") {
		return bmOne;
	}
	else if(str == "black") {
		return bmOneMinusAlpha;
	}
	else{
		throw megaglest_runtime_error("Unknown particle mode: " + str);
	}
}

// =============== SET ==========================

void ParticleSystem::setState(State state){
	this->state= state;
	for(int i=getChildCount()-1; i>=0; i--)
		getChild(i)->setState(state);
}

void ParticleSystem::setTexture(Texture *texture){
	this->texture= texture;
}

void ParticleSystem::setPos(Vec3f pos){
	this->pos= pos;
	for(int i=getChildCount()-1; i>=0; i--)
		getChild(i)->setPos(pos);
}

void ParticleSystem::setColor(Vec4f color){
	this->color= color;
}

void ParticleSystem::setColorNoEnergy(Vec4f colorNoEnergy){
	this->colorNoEnergy= colorNoEnergy;
}

void ParticleSystem::setEmissionRate(float emissionRate){
	this->emissionRate= emissionRate;
	this->emissionRate = truncateDecimal<float>(this->emissionRate,6);
}

void ParticleSystem::setMaxParticleEnergy(int maxParticleEnergy){
	this->maxParticleEnergy= maxParticleEnergy;
}

void ParticleSystem::setVarParticleEnergy(int varParticleEnergy){
	this->varParticleEnergy= varParticleEnergy;
}

void ParticleSystem::setParticleSize(float particleSize){
	this->particleSize= particleSize;
	this->particleSize = truncateDecimal<float>(this->particleSize,6);
}

void ParticleSystem::setSpeed(float speed){
	this->speed= speed;
	this->speed = truncateDecimal<float>(this->speed,6);
}

void ParticleSystem::setSpeedUpRelative(float speedUpRelative){
	this->speedUpRelative= speedUpRelative;
	this->speedUpRelative = truncateDecimal<float>(this->speedUpRelative,6);
}

void ParticleSystem::setSpeedUpConstant(float speedUpConstant){
	this->speedUpConstant= speedUpConstant;
	this->speedUpConstant = truncateDecimal<float>(this->speedUpConstant,6);
}

void ParticleSystem::setActive(bool active){
	this->active= active;
	for(int i=getChildCount()-1; i>=0; i--)
		getChild(i)->setActive(active);
}

void ParticleSystem::setObserver(ParticleObserver *particleObserver){
	this->particleObserver= particleObserver;
}

ParticleSystem* ParticleSystem::getChild(int i){
	throw std::out_of_range("ParticleSystem::getChild bad");
}

void ParticleSystem::setVisible(bool visible){
	this->visible= visible;
	for(int i=getChildCount()-1; i>=0; i--) {
		getChild(i)->setVisible(visible);
	}
}

string ParticleSystem::toString() const {
	string result = "ParticleSystem ";

	result += "particles = " + intToStr(particles.size());

//	for(unsigned int i = 0; i < particles.size(); ++i) {
//		Particle &particle = particles[i];
//
//	}

	result += "\nrandom = " + intToStr(random.getLastNumber());

	result += "\nblendMode = " + intToStr(blendMode);
	result += "\nstate = " + intToStr(state);
	result += "\nactive = " + intToStr(active);
	//result += "\nvisible = " + intToStr(visible);
	result += "\naliveParticleCount = " + intToStr(aliveParticleCount);
	result += "\nparticleCount = " + intToStr(particleCount);

	result += "\ntextureFileLoadDeferred = " + textureFileLoadDeferred;
	result += "\ntextureFileLoadDeferredFormat = " + intToStr(textureFileLoadDeferredFormat);
	result += "\ntextureFileLoadDeferredComponents = " + intToStr(textureFileLoadDeferredComponents);

	if(texture != NULL) {
		result += "\ntexture = " + extractFileFromDirectoryPath(texture->getPath());
	}
	result += "\npos = " + pos.getString();
	result += "\ncolor = " + color.getString();
	result += "\ncolorNoEnergy = " + colorNoEnergy.getString();
	result += "\nemissionRate = " + floatToStr(emissionRate,6);
	result += "\nemissionState = " + floatToStr(emissionState,6);
	result += "\nmaxParticleEnergy = " + intToStr(maxParticleEnergy);
	result += "\nvarParticleEnergy = " + intToStr(varParticleEnergy);
	result += "\nparticleSize = " + floatToStr(particleSize,6);
	result += "\nspeed = " + floatToStr(speed,6);
	result += "\nfactionColor = " + factionColor.getString();
    result += "\nteamcolorNoEnergy = " + intToStr(teamcolorNoEnergy);
    result += "\nteamcolorEnergy = " + intToStr(teamcolorEnergy);
	result += "\nalternations = " + intToStr(alternations);
	result += "\nparticleSystemStartDelay = " + intToStr(particleSystemStartDelay);
	//ParticleObserver *particleObserver;

	return result;
}

void ParticleSystem::saveGame(XmlNode *rootNode) {
	std::map<string,string> mapTagReplacements;
	XmlNode *particleSystemNode = rootNode->addChild("ParticleSystem");

//	std::vector<Particle> particles;
//	for(unsigned int i = 0; i < particles.size(); ++i) {
//		Particle &particle = particles[i];
//		particle.saveGame(particleSystemNode);
//	}
//	RandomGen random;
	particleSystemNode->addAttribute("random",intToStr(random.getLastNumber()), mapTagReplacements);

//	BlendMode blendMode;
	particleSystemNode->addAttribute("blendMode",intToStr(blendMode), mapTagReplacements);
//	State state;
	particleSystemNode->addAttribute("state",intToStr(state), mapTagReplacements);
//	bool active;
	particleSystemNode->addAttribute("active",intToStr(active), mapTagReplacements);
//	bool visible;
	particleSystemNode->addAttribute("visible",intToStr(visible), mapTagReplacements);
//	int aliveParticleCount;
	particleSystemNode->addAttribute("aliveParticleCount",intToStr(aliveParticleCount), mapTagReplacements);
//	int particleCount;
	particleSystemNode->addAttribute("particleCount",intToStr(particleCount), mapTagReplacements);
//
//	Texture *texture;
	if(texture != NULL) {
		particleSystemNode->addAttribute("texture",texture->getPath(), mapTagReplacements);
		particleSystemNode->addAttribute("textureid",intToStr(texture->getTextureSystemId()), mapTagReplacements);
		particleSystemNode->addAttribute("textureFormat",intToStr(texture->getFormat()), mapTagReplacements);
		Texture2D *t2d = dynamic_cast<Texture2D *>(texture);
		if(t2d != NULL && t2d->getPixmapConst() != NULL) {
			particleSystemNode->addAttribute("textureComponents",intToStr(t2d->getPixmapConst()->getComponents()), mapTagReplacements);
		}
	}
//	Vec3f pos;
	particleSystemNode->addAttribute("pos",pos.getString(), mapTagReplacements);
//	Vec4f color;
	particleSystemNode->addAttribute("color",color.getString(), mapTagReplacements);
//	Vec4f colorNoEnergy;
	particleSystemNode->addAttribute("colorNoEnergy",colorNoEnergy.getString(), mapTagReplacements);
//	float emissionRate;
	particleSystemNode->addAttribute("emissionRate",floatToStr(emissionRate,6), mapTagReplacements);
//	float emissionState;
	particleSystemNode->addAttribute("emissionState",floatToStr(emissionState,6), mapTagReplacements);
//	int maxParticleEnergy;
	particleSystemNode->addAttribute("maxParticleEnergy",intToStr(maxParticleEnergy), mapTagReplacements);
//	int varParticleEnergy;
	particleSystemNode->addAttribute("varParticleEnergy",intToStr(varParticleEnergy), mapTagReplacements);
//	float particleSize;
	particleSystemNode->addAttribute("particleSize",floatToStr(particleSize,6), mapTagReplacements);
//	float speed;
	particleSystemNode->addAttribute("speed",floatToStr(speed,6), mapTagReplacements);
//	Vec3f factionColor;
	particleSystemNode->addAttribute("factionColor",factionColor.getString(), mapTagReplacements);
//    bool teamcolorNoEnergy;
	particleSystemNode->addAttribute("teamcolorNoEnergy",intToStr(teamcolorNoEnergy), mapTagReplacements);
//    bool teamcolorEnergy;
	particleSystemNode->addAttribute("teamcolorEnergy",intToStr(teamcolorEnergy), mapTagReplacements);
//	int alternations;
	particleSystemNode->addAttribute("alternations",intToStr(alternations), mapTagReplacements);
//	int particleSystemStartDelay;
	particleSystemNode->addAttribute("particleSystemStartDelay",intToStr(particleSystemStartDelay), mapTagReplacements);
//	ParticleObserver *particleObserver;
	if(particleObserver != NULL) {
		particleObserver->saveGame(particleSystemNode);
	}
}

string ParticleSystem::getTextureFileLoadDeferred() {
	return textureFileLoadDeferred;
}
int ParticleSystem::getTextureFileLoadDeferredSystemId() {
	return textureFileLoadDeferredSystemId;
}
Texture::Format ParticleSystem::getTextureFileLoadDeferredFormat() {
	return textureFileLoadDeferredFormat;
}
int ParticleSystem::getTextureFileLoadDeferredComponents() {
	return textureFileLoadDeferredComponents;
}

void ParticleSystem::loadGame(const XmlNode *rootNode) {
	const XmlNode *particleSystemNode = rootNode->getChild("ParticleSystem");

	particleCount = particleSystemNode->getAttribute("particleCount")->getIntValue();
	//printf("Load Smoke particle (ParticleSystem)\n");
	//	std::vector<Particle> particles;
//	for(unsigned int i = 0; i < particles.size(); ++i) {
//		Particle &particle = particles[i];
//		particle.saveGame(particleSystemNode);
//	}

	particles.clear();
	particles.resize(particleCount);

//	vector<XmlNode *> particleNodeList = particleSystemNode->getChildList("Particle");
//	for(unsigned int i = 0; i < particleNodeList.size(); ++i) {
//		XmlNode *node = particleNodeList[i];
//
//		//printf("Load Smoke particle (Particle = %d)\n",i);
//
//		Particle particle;
//		particle.loadGame(node);
//		particles.push_back(particle);
//	}

	//	RandomGen random;
	random.setLastNumber(particleSystemNode->getAttribute("random")->getIntValue());

	//	BlendMode blendMode;
	blendMode = static_cast<BlendMode>(particleSystemNode->getAttribute("blendMode")->getIntValue());
	//	State state;
	state = static_cast<State>(particleSystemNode->getAttribute("state")->getIntValue());
	//	bool active;
	active = particleSystemNode->getAttribute("active")->getIntValue() != 0;
	//	bool visible;
	visible = particleSystemNode->getAttribute("visible")->getIntValue() != 0;
	//	int aliveParticleCount;
	aliveParticleCount = particleSystemNode->getAttribute("aliveParticleCount")->getIntValue();
	//	int particleCount;
	particleCount = particleSystemNode->getAttribute("particleCount")->getIntValue();
	//
	//	Texture *texture;
	if(particleSystemNode->hasAttribute("texture") == true) {
		textureFileLoadDeferred = particleSystemNode->getAttribute("texture")->getValue();
		textureFileLoadDeferredSystemId = particleSystemNode->getAttribute("textureid")->getIntValue();

		textureFileLoadDeferredFormat = static_cast<Texture::Format>(particleSystemNode->getAttribute("textureFormat")->getIntValue());
		if(particleSystemNode->hasAttribute("textureComponents") == true) {
			textureFileLoadDeferredComponents = particleSystemNode->getAttribute("textureComponents")->getIntValue();
		}
	}

	//	Vec3f pos;
	pos = Vec3f::strToVec3(particleSystemNode->getAttribute("pos")->getValue());
	//	Vec4f color;
	color = Vec4f::strToVec4(particleSystemNode->getAttribute("color")->getValue());
	//	Vec4f colorNoEnergy;
	colorNoEnergy = Vec4f::strToVec4(particleSystemNode->getAttribute("colorNoEnergy")->getValue());
	//	float emissionRate;
	emissionRate = particleSystemNode->getAttribute("emissionRate")->getFloatValue();
	//	float emissionState;
	emissionState = particleSystemNode->getAttribute("emissionState")->getFloatValue();
	//	int maxParticleEnergy;
	maxParticleEnergy = particleSystemNode->getAttribute("maxParticleEnergy")->getIntValue();
	//	int varParticleEnergy;
	varParticleEnergy = particleSystemNode->getAttribute("varParticleEnergy")->getIntValue();
	//	float particleSize;
	particleSize = particleSystemNode->getAttribute("particleSize")->getFloatValue();
	//	float speed;
	speed = particleSystemNode->getAttribute("speed")->getFloatValue();
	//	Vec3f factionColor;
	factionColor = Vec3f::strToVec3(particleSystemNode->getAttribute("factionColor")->getValue());
	//    bool teamcolorNoEnergy;
	teamcolorNoEnergy = particleSystemNode->getAttribute("teamcolorNoEnergy")->getIntValue() != 0;
	//    bool teamcolorEnergy;
	teamcolorEnergy = particleSystemNode->getAttribute("teamcolorEnergy")->getIntValue() != 0;
	//	int alternations;
	alternations = particleSystemNode->getAttribute("alternations")->getIntValue();
	//	int particleSystemStartDelay;
	particleSystemStartDelay = particleSystemNode->getAttribute("particleSystemStartDelay")->getIntValue();

	//	ParticleObserver *particleObserver;
	//if(particleObserver != NULL) {
	//	particleObserver->loadGame(particleSystemNode);
	//}
}

// =============== MISC =========================
void ParticleSystem::fade(){
	//printf("**************Fading particle System:\n[%s]\n",this->toString().c_str());

	bool alreadyFading = (state == sFade);
	if(particleObserver != NULL){
		if(state != sPlay) {
			char szBuf[8096]="";
			snprintf(szBuf,8096,"state != sPlay, state = [%d]\n",state);
			//throw megaglest_runtime_error(szBuf);
			//printf(szBuf);
			SystemFlags::OutputDebug(SystemFlags::debugError,"%s",szBuf);
		}
		//assert(state == sPlay);
	}

	state= sFade;
	if(alreadyFading == false) {
		if(particleObserver != NULL){
			particleObserver->update(this);
			particleObserver=NULL;
		}
		for(int i=getChildCount()-1; i>=0; i--) {
			getChild(i)->fade();
		}
	}
}

int ParticleSystem::isEmpty() const {
	//assert(aliveParticleCount>=0);
	return aliveParticleCount == 0 && state != sPause;
}

// =============== PROTECTED =========================

// if there is one dead particle it returns it else, return the particle with 
// less energy
Particle * ParticleSystem::createParticle() {

	//if any dead particles
	if(aliveParticleCount < particleCount) {
		++aliveParticleCount;
		return &particles[aliveParticleCount - 1];
	}

	//if not
	int minEnergy= particles[0].energy;
	int minEnergyParticle= 0;

	for(int i= 0; i < particleCount; ++i){
		if(particles[i].energy < minEnergy){
			minEnergy= particles[i].energy;
			minEnergyParticle= i;
		}
	}
	return &particles[minEnergyParticle];
}

void ParticleSystem::initParticle(Particle *p, int particleIndex) {
	p->pos= pos;
	p->lastPos= p->pos;
	p->speed= Vec3f(0.0f);
	p->accel= Vec3f(0.0f);
	p->color= Vec4f(1.0f, 1.0f, 1.0f, 1.0);
	p->size= particleSize;
	p->size = truncateDecimal<float>(p->size,6);
	p->energy= maxParticleEnergy + random.randRange(-varParticleEnergy, varParticleEnergy);
}

void ParticleSystem::updateParticle(Particle *p) {
	p->lastPos= p->pos;
	p->pos= p->pos + p->speed;
	p->speed= p->speed + p->accel;
	p->energy--;
}

bool ParticleSystem::deathTest(Particle *p) {
	return p->energy <= 0;
}

void ParticleSystem::killParticle(Particle *p) {
	aliveParticleCount--;
}

void ParticleSystem::setFactionColor(Vec3f factionColor){
	this->factionColor= factionColor;
	Vec3f tmpCol;

	if(teamcolorEnergy){
		this->color= Vec4f(factionColor.x, factionColor.y, factionColor.z, this->color.w);
	}
	if(teamcolorNoEnergy){
		this->colorNoEnergy= Vec4f(factionColor.x, factionColor.y, factionColor.z, this->colorNoEnergy.w);
	}
	for(int i=getChildCount()-1; i>=0; i--)
		getChild(i)->setFactionColor(factionColor);
}

// ===========================================================================
//  FireParticleSystem
// ===========================================================================


FireParticleSystem::FireParticleSystem(int particleCount) :
	ParticleSystem(particleCount){

	radius= 0.5f;
	speed= 0.01f;
	windSpeed= Vec3f(0.0f);

	setParticleSize(0.6f);
	setColorNoEnergy(Vec4f(1.0f, 0.5f, 0.0f, 1.0f));
}

void FireParticleSystem::initParticle(Particle *p, int particleIndex){
	ParticleSystem::initParticle(p, particleIndex);

	float ang= random.randRange(-2.0f * pi, 2.0f * pi);
#ifdef USE_STREFLOP
	float mod= streflop::fabsf(static_cast<streflop::Simple>(random.randRange(-radius, radius)));

	float x= streflop::sinf(static_cast<streflop::Simple>(ang))*mod;
	float y= streflop::cosf(static_cast<streflop::Simple>(ang))*mod;

	float radRatio= streflop::sqrtf(static_cast<streflop::Simple>(mod/radius));
#else
	float mod= fabsf(random.randRange(-radius, radius));

	float x= sinf(ang) * mod;
	float y= cosf(ang) * mod;

	float radRatio= sqrtf((mod / radius));
#endif

	p->color= colorNoEnergy * 0.5f + colorNoEnergy * 0.5f * radRatio;
	p->energy= static_cast<int> (maxParticleEnergy * radRatio)
	        + random.randRange(-varParticleEnergy, varParticleEnergy);
	p->pos= Vec3f(pos.x + x, pos.y + random.randRange(-radius / 2, radius / 2), pos.z + y);
	p->pos.x = truncateDecimal<float>(p->pos.x,6);
	p->pos.y = truncateDecimal<float>(p->pos.y,6);
	p->pos.z = truncateDecimal<float>(p->pos.z,6);

	p->lastPos= pos;
	p->size= particleSize;
	p->speed= Vec3f(0, speed + speed * random.randRange(-0.5f, 0.5f), 0) + windSpeed;
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);

}

void FireParticleSystem::updateParticle(Particle *p){
	p->lastPos= p->pos;
	p->pos= p->pos + p->speed;
	p->energy--;

	if(p->color.x > 0.0f)
		p->color.x*= 0.98f;
	if(p->color.y > 0.0f)
		p->color.y*= 0.98f;
	if(p->color.w > 0.0f)
		p->color.w*= 0.98f;

	p->speed.x*= 1.001f;
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);

}

string FireParticleSystem::toString() const {
	string result = ParticleSystem::toString();

	result += "\nFireParticleSystem ";
	result += "\nradius = " + floatToStr(radius);
	result += "\nwindSpeed = " + windSpeed.getString();

	return result;
}
// ================= SET PARAMS ====================

void FireParticleSystem::setRadius(float radius){
	this->radius= radius;
}

void FireParticleSystem::setWind(float windAngle, float windSpeed) {
#ifdef USE_STREFLOP
	this->windSpeed.x= streflop::sinf(static_cast<streflop::Simple>(degToRad(windAngle)))*windSpeed;
	this->windSpeed.y= 0.0f;
	this->windSpeed.z= streflop::cosf(static_cast<streflop::Simple>(degToRad(windAngle)))*windSpeed;
#else
	this->windSpeed.x= sinf(degToRad(windAngle)) * windSpeed;
	this->windSpeed.y= 0.0f;
	this->windSpeed.z= cosf(degToRad(windAngle)) * windSpeed;
#endif

	this->windSpeed.x = truncateDecimal<float>(this->windSpeed.x,6);
	this->windSpeed.y = truncateDecimal<float>(this->windSpeed.y,6);
	this->windSpeed.z = truncateDecimal<float>(this->windSpeed.z,6);
}

void FireParticleSystem::saveGame(XmlNode *rootNode) {
	std::map<string,string> mapTagReplacements;
	XmlNode *fireParticleSystemNode = rootNode->addChild("FireParticleSystem");

	ParticleSystem::saveGame(fireParticleSystemNode);

//	float radius;
	fireParticleSystemNode->addAttribute("radius",floatToStr(radius,6), mapTagReplacements);
//	Vec3f windSpeed;
	fireParticleSystemNode->addAttribute("windSpeed",windSpeed.getString(), mapTagReplacements);
}
void FireParticleSystem::loadGame(const XmlNode *rootNode) {
	const XmlNode *fireParticleSystemNode = rootNode;

	ParticleSystem::loadGame(fireParticleSystemNode);

//	float radius;
	radius = fireParticleSystemNode->getAttribute("radius")->getFloatValue();
//	Vec3f windSpeed;
	windSpeed = Vec3f::strToVec3(fireParticleSystemNode->getAttribute("windSpeed")->getValue());
}

Checksum FireParticleSystem::getCRC() {
	Checksum crcForParticleSystem = ParticleSystem::getCRC();

	//float radius;
	//Vec3f windSpeed;

	return crcForParticleSystem;
}

// ===========================================================================
//  GameParticleSystem
// ===========================================================================

GameParticleSystem::GameParticleSystem(int particleCount):
	ParticleSystem(particleCount),
	primitive(pQuad),
	model(NULL),
	modelCycle(0.0f),
	offset(0.0f),
	direction(0.0f, 1.0f, 0.0f),
	tween(0.0f)
{}

GameParticleSystem::~GameParticleSystem(){
	for(Children::iterator it= children.begin(); it != children.end(); ++it){
		(*it)->setParent(NULL);
		(*it)->fade();
	}
}

GameParticleSystem::Primitive GameParticleSystem::strToPrimitive(const string &str){
	if(str == "quad"){
		return pQuad;
	}
	else if(str == "line"){
		return pLine;
	}
	else{
		throw megaglest_runtime_error("Unknown particle primitive: " + str);
	}
}

int GameParticleSystem::getChildCount(){
	return (int)children.size();
}

ParticleSystem* GameParticleSystem::getChild(int i){
	return children.at(i); // does bounds checking
}

void GameParticleSystem::addChild(UnitParticleSystem* child) {
	assert(!child->getParent());
	child->setParent(this);
	children.push_back(child);
}

void GameParticleSystem::removeChild(UnitParticleSystem* child){
	assert(this == child->getParent());
	Children::iterator it = std::find(children.begin(),children.end(),child);
	assert(it != children.end());
	children.erase(it);
}

void GameParticleSystem::setPos(Vec3f pos){
	this->pos= pos;
	positionChildren();
}

void GameParticleSystem::positionChildren() {
	Vec3f child_pos = pos - offset;
	for(int i=getChildCount()-1; i>=0; i--)
		getChild(i)->setPos(child_pos);
}

void GameParticleSystem::setOffset(Vec3f offset){
	this->offset= offset;
	positionChildren();
}

void GameParticleSystem::render(ParticleRenderer *pr, ModelRenderer *mr){
	if(active){
		if(model != NULL){
			pr->renderModel(this, mr);
		}
		switch(primitive){
			case pQuad:
				pr->renderSystem(this);
				break;
			case pLine:
				pr->renderSystemLine(this);
				break;
			default:
				assert(false);
				break;
		}
	}
}

void GameParticleSystem::setTween(float relative,float absolute) {
	if(model) {
		// animation?
		//printf("#1 Particle model meshcount [%d] modelCycle = %f, relative = %f, absolute = %f\n",model->getMeshCount(),modelCycle,relative,absolute);
		if(modelCycle == 0.0f) {
			tween= relative;
		}
		else {
		#ifdef USE_STREFLOP
			if(streflop::fabs(static_cast<streflop::Simple>(absolute)) <= 0.00001f){
		#else
			if(fabs(absolute) <= 0.00001f) {
		#endif
				tween = 0.0f;
			}
			else {
		#ifdef USE_STREFLOP
				tween= streflop::fmod(static_cast<streflop::Simple>(absolute), static_cast<streflop::Simple>(modelCycle));
		#else
				tween= fmod(absolute, modelCycle);
		#endif
				tween /= modelCycle;
			}
		}

		tween = truncateDecimal<float>(tween,6);
		if(tween < 0.0f || tween > 1.0f) {
			//printf("In [%s::%s Line: %d] WARNING setting tween to [%f] clamping tween, modelCycle [%f] absolute [%f] relative [%f]\n",__FILE__,__FUNCTION__,__LINE__,tween,modelCycle,absolute,relative);
			//assert(tween >= 0.0f && tween <= 1.0f);
		}

		tween= clamp(tween, 0.0f, 1.0f);

		//printf("#2 Particle model meshcount [%d] modelCycle = %f, relative = %f, absolute = %f\n",model->getMeshCount(),modelCycle,relative,absolute);
	}
	for(Children::iterator it= children.begin(); it != children.end(); ++it) {
		(*it)->setTween(relative,absolute);
	}
}

string GameParticleSystem::getModelFileLoadDeferred() {
	return modelFileLoadDeferred;
}

void GameParticleSystem::saveGame(XmlNode *rootNode) {
	std::map<string,string> mapTagReplacements;
	XmlNode *gameParticleSystemNode = rootNode->addChild("GameParticleSystem");

	ParticleSystem::saveGame(gameParticleSystemNode);

//	Children children;
	for(unsigned int i = 0; i < children.size(); ++i) {
		if(children[i] != NULL) {
			children[i]->saveGame(gameParticleSystemNode);
		}
	}
//	Primitive primitive;
	gameParticleSystemNode->addAttribute("primitive",intToStr(primitive), mapTagReplacements);
//	Model *model;
	if(model != NULL) {
		gameParticleSystemNode->addAttribute("model",model->getFileName(), mapTagReplacements);
	}
//	float modelCycle;
	gameParticleSystemNode->addAttribute("modelCycle",floatToStr(modelCycle,6), mapTagReplacements);
//	Vec3f offset;
	gameParticleSystemNode->addAttribute("offset",offset.getString(), mapTagReplacements);
//	Vec3f direction;
	gameParticleSystemNode->addAttribute("direction",direction.getString(), mapTagReplacements);
//	float tween;
	gameParticleSystemNode->addAttribute("tween",floatToStr(tween,6), mapTagReplacements);
}
void GameParticleSystem::loadGame(const XmlNode *rootNode) {
	const XmlNode *gameParticleSystemNode = rootNode->getChild("GameParticleSystem");

	//printf("Load Smoke particle (GameParticleSystem)\n");
	ParticleSystem::loadGame(gameParticleSystemNode);

	//	Children children;
//	for(unsigned int i = 0; i < children.size(); ++i) {
//		children[i]->saveGame(gameParticleSystemNode);
//	}
	vector<XmlNode *> childrenNodeList = gameParticleSystemNode->getChildList("UnitParticleSystem");
	for(unsigned int i = 0; i < childrenNodeList.size(); ++i) {
		XmlNode *node = childrenNodeList[i];

		UnitParticleSystem *ups = new UnitParticleSystem();
		//ups->setParticleOwner(!!!);
		ups->loadGame(node);

		//children.push_back(ups);
		addChild(ups);
	}

	//	Primitive primitive;
	primitive = static_cast<Primitive>(gameParticleSystemNode->getAttribute("primitive")->getIntValue());
	//	Model *model;
	//if(model != NULL) {
	//	gameParticleSystemNode->addAttribute("model",model->getFileName(), mapTagReplacements);
	//}
	if(gameParticleSystemNode->hasAttribute("model") == true) {
		modelFileLoadDeferred = gameParticleSystemNode->getAttribute("model")->getValue();
	}

	//	float modelCycle;
	//gameParticleSystemNode->addAttribute("modelCycle",floatToStr(modelCycle), mapTagReplacements);
	modelCycle = gameParticleSystemNode->getAttribute("modelCycle")->getFloatValue();
	//	Vec3f offset;
	offset = Vec3f::strToVec3(gameParticleSystemNode->getAttribute("offset")->getValue());
	//	Vec3f direction;
	direction = Vec3f::strToVec3(gameParticleSystemNode->getAttribute("direction")->getValue());
	//	float tween;
	tween = gameParticleSystemNode->getAttribute("tween")->getFloatValue();
}

Checksum GameParticleSystem::getCRC() {
	Checksum crcForParticleSystem = ParticleSystem::getCRC();

	return crcForParticleSystem;
}

string GameParticleSystem::toString() const {
	string result = ParticleSystem::toString();

	result += "\nGameParticleSystem ";
	result += "\nchildren = " + intToStr(children.size());
	result += "\nprimitive = " + intToStr(primitive);

	//string modelFileLoadDeferred;
	//Model *model;
	result += "\nmodelCycle = " + floatToStr(modelCycle);
	result += "\noffset = " + offset.getString();
	result += "\ndirection = " + direction.getString();
	result += "\ntween = " + floatToStr(tween);

	return result;
}

// ===========================================================================
//  UnitParticleSystem
// ===========================================================================
bool UnitParticleSystem::isNight= false;
Vec3f UnitParticleSystem::lightColor=Vec3f(1.0f,1.0f,1.0f);

UnitParticleSystem::UnitParticleSystem(int particleCount) :
		GameParticleSystem(particleCount),	parent(NULL) {

	particleSystemType = NULL;
	radius= 0.5f;
	speed= 0.01f;
	windSpeed= Vec3f(0.0f);
	minRadius = 0.0;

	setParticleSize(0.6f);
	setColorNoEnergy(Vec4f(1.0f, 0.5f, 0.0f, 1.0f));
	sizeNoEnergy=1.0f;

	primitive= pQuad;
	gravity= 0.0f;

	fixed= false;
	shape = UnitParticleSystem::sLinear;
	angle= 0.0f;
	rotation= 0.0f;
	relativeDirection= true;
	relative= false;
	staticParticleCount= 0;

	isVisibleAtNight= true;
	isVisibleAtDay= true;
	isDaylightAffected= false;

	cRotation= Vec3f(1.0f, 1.0f, 1.0f);
	fixedAddition= Vec3f(0.0f, 0.0f, 0.0f);
	//prepare system for given staticParticleCount
	if(staticParticleCount > 0){
		emissionState= (float) staticParticleCount;
	}
	energyUp= false;
	
	delay = 0; // none
	lifetime = -1; // forever
	emissionRateFade=0.0f;

	startTime = 0;
	endTime = 1;
	unitModel=NULL;
	meshName="";
	meshPos=Vec3f(0,0,0);

	radiusBasedStartenergy = false;
}

UnitParticleSystem::~UnitParticleSystem(){
	if(parent){
		parent->removeChild(this);
	}
}

bool UnitParticleSystem::getVisible() const{
	if((isNight==true) && (isVisibleAtNight==true)){
		return visible;
	}
	else if((isNight==false) && (isVisibleAtDay==true)){
		return visible;
	}
	else
		return false;
}

void UnitParticleSystem::render(ParticleRenderer *pr, ModelRenderer *mr) {
	GameParticleSystem::render(pr,mr);
}

void UnitParticleSystem::setRotation(float rotation){
	this->rotation= rotation;
	for(Children::iterator it= children.begin(); it != children.end(); ++it)
		(*it)->setRotation(rotation);
}

void UnitParticleSystem::fade(){
	if(!parent || (lifetime<=0 && !(emissionRateFade && emissionRate > 0))){ // particle has its own lifetime?
		unitModel=NULL;
		GameParticleSystem::fade();
	}
}

UnitParticleSystem::Shape UnitParticleSystem::strToShape(const string& str){
	if(str == "spherical"){
		return sSpherical;
	}
	else if(str == "conical"){
		return sConical;
	}
	else if(str == "linear"){
		return sLinear;
	}
	else{
		throw megaglest_runtime_error("Unknown particle shape: " + str);
	}
}

void UnitParticleSystem::initParticle(Particle *p, int particleIndex){
	ParticleSystem::initParticle(p, particleIndex);

	const float ang= random.randRange(-2.0f * pi, 2.0f * pi);
#ifdef USE_STREFLOP	
	const float mod= streflop::fabsf(static_cast<streflop::Simple>(random.randRange(-radius, radius)));
	const float radRatio= streflop::sqrtf(static_cast<streflop::Simple>(mod/radius));
#else
	const float mod= fabsf(random.randRange(-radius, radius));
	const float radRatio= sqrtf(mod / radius);
#endif
	p->color= color;

	if(isDaylightAffected==true) {
		p->color.x=p->color.x*lightColor.x;
		p->color.y=p->color.y*lightColor.y;
		p->color.z=p->color.z*lightColor.z;
	}
	if(radiusBasedStartenergy == true) {
		p->energy= static_cast<int> (maxParticleEnergy * radRatio) +
				random.randRange(-varParticleEnergy,varParticleEnergy);
	}
	else {
		p->energy= static_cast<int> (maxParticleEnergy) +
				random.randRange(-varParticleEnergy, varParticleEnergy);
	}

	p->lastPos= pos;
	oldPosition= pos;
	p->size= particleSize;
	p->speedUpRelative= speedUpRelative;
	p->accel= Vec3f(0.0f, -gravity, 0.0f);
	p->accel.x = truncateDecimal<float>(p->accel.x,6);
	p->accel.y = truncateDecimal<float>(p->accel.y,6);
	p->accel.z = truncateDecimal<float>(p->accel.z,6);
	
	// work out where we start for our shape (set speed and pos)
	switch(shape){
	case sSpherical:
		angle = (float)random.randRange(0,360);
		// fall through
	case sConical:{
		Vec2f horiz = Vec2f(1,0).rotate(ang);
		Vec2f vert = Vec2f(1,0).rotate(degToRad(angle));
		Vec3f start = Vec3f(horiz.x*vert.y,vert.x,horiz.y).getNormalized(); // close enough
		p->speed = start * speed;
		p->speed.x = truncateDecimal<float>(p->speed.x,6);
		p->speed.y = truncateDecimal<float>(p->speed.y,6);
		p->speed.z = truncateDecimal<float>(p->speed.z,6);

		start = start * random.randRange(minRadius,radius);
		p->pos = pos + offset + start;
		p->pos.x = truncateDecimal<float>(p->pos.x,6);
		p->pos.y = truncateDecimal<float>(p->pos.y,6);
		p->pos.z = truncateDecimal<float>(p->pos.z,6);

	} break;
	case sLinear:{
	#ifdef USE_STREFLOP	
		float x= streflop::sinf(static_cast<streflop::Simple>(ang))*mod;
		float y= streflop::cosf(static_cast<streflop::Simple>(ang))*mod;
	#else	
		float x= sinf(ang) * mod;
		float y= cosf(ang) * mod;
	#endif
		const float rad= degToRad(rotation);
		if(!relative){
			p->pos= Vec3f(pos.x + x + offset.x, pos.y +
					random.randRange(-radius / 2, radius / 2) + offset.y,
					pos.z + y + offset.z);
			p->pos.x = truncateDecimal<float>(p->pos.x,6);
			p->pos.y = truncateDecimal<float>(p->pos.y,6);
			p->pos.z = truncateDecimal<float>(p->pos.z,6);

		}
		else {
			Vec3f combinedOffset=Vec3f(offset);
			if(meshName!=""){
				combinedOffset.x+=meshPos.x;
				combinedOffset.y+=meshPos.y;
				combinedOffset.z+=meshPos.z;
				if(meshPos.x==0 && meshPos.y==0 && meshPos.z==0) {
					printf("meshPosFail\n");
				}
			}
			// rotate it according to rotation
	#ifdef USE_STREFLOP
			p->pos= Vec3f(pos.x+x+combinedOffset.z*streflop::sinf(static_cast<streflop::Simple>(rad))+combinedOffset.x*streflop::cosf(static_cast<streflop::Simple>(rad)), pos.y+random.randRange(-radius/2, radius/2)+combinedOffset.y, pos.z+y+(combinedOffset.z*streflop::cosf(static_cast<streflop::Simple>(rad))-combinedOffset.x*streflop::sinf(static_cast<streflop::Simple>(rad))));
	#else
			p->pos= Vec3f(pos.x + x + combinedOffset.z * sinf(rad) + combinedOffset.x * cosf(rad), pos.y + random.randRange(-radius / 2,
				radius / 2) + combinedOffset.y, pos.z + y + (combinedOffset.z * cosf(rad) - combinedOffset.x * sinf(rad)));
	#endif

			p->pos.x = truncateDecimal<float>(p->pos.x,6);
			p->pos.y = truncateDecimal<float>(p->pos.y,6);
			p->pos.z = truncateDecimal<float>(p->pos.z,6);

		}
		p->speed= Vec3f(direction.x + direction.x * random.randRange(-0.5f, 0.5f), direction.y + direction.y
			* random.randRange(-0.5f, 0.5f), direction.z + direction.z * random.randRange(-0.5f, 0.5f));
		p->speed= p->speed * speed;

		p->speed.x = truncateDecimal<float>(p->speed.x,6);
		p->speed.y = truncateDecimal<float>(p->speed.y,6);
		p->speed.z = truncateDecimal<float>(p->speed.z,6);

		if(relative && relativeDirection) {
	#ifdef USE_STREFLOP
			p->speed=Vec3f(p->speed.z*streflop::sinf(
					static_cast<streflop::Simple>(rad)) + p->speed.x *
					streflop::cosf(static_cast<streflop::Simple>(rad)),
					p->speed.y, (p->speed.z * streflop::cosf(
							static_cast<streflop::Simple>(rad)) - p->speed.x *
							streflop::sinf(static_cast<streflop::Simple>(rad))));
	#else
			p->speed= Vec3f(p->speed.z * sinf(rad) + p->speed.x * cosf(rad),
					p->speed.y, (p->speed.z * cosf(rad) - p->speed.x * sinf(rad)));
	#endif

			p->speed.x = truncateDecimal<float>(p->speed.x,6);
			p->speed.y = truncateDecimal<float>(p->speed.y,6);
			p->speed.z = truncateDecimal<float>(p->speed.z,6);
		}
	} break;
	default: throw megaglest_runtime_error("bad shape");
	}
	//need to do that down here because we need p->speed for it.
	p->speedUpConstant= Vec3f(speedUpConstant)*p->speed;
}

void UnitParticleSystem::update(){
	// delay and timeline are only applicable for child particles
	if(parent && delay > 0 && delay--) {
		return;
	}
	if(parent && lifetime > 0 && !--lifetime) {
		fade();
	}
	if(state != sPause) {
		emissionRate-= emissionRateFade;
		emissionRate = truncateDecimal<float>(emissionRate,6);

		if(parent && emissionRate < 0.0f) {
			fade();
		}
	}
	if(fixed) {
		fixedAddition= Vec3f(pos.x - oldPosition.x, pos.y - oldPosition.y, pos.z - oldPosition.z);
		fixedAddition.x = truncateDecimal<float>(fixedAddition.x,6);
		fixedAddition.y = truncateDecimal<float>(fixedAddition.y,6);
		fixedAddition.z = truncateDecimal<float>(fixedAddition.z,6);

		oldPosition= pos;
	}
	ParticleSystem::update();
}

void UnitParticleSystem::updateParticle(Particle *p){
	float energyRatio;
	if(alternations > 0){
		int interval= (maxParticleEnergy / alternations);
		float moduloValue= (float)((int)(static_cast<float> (p->energy)) % interval);
		float floatInterval=static_cast<float> (interval);

		if(moduloValue < floatInterval / 2.0f){
			energyRatio= (floatInterval - moduloValue) / floatInterval;
		}
		else{
			energyRatio= moduloValue / floatInterval;
		}
		energyRatio= clamp(energyRatio, 0.f, 1.f);
	}
	else{
		energyRatio= clamp(static_cast<float> (p->energy) / static_cast<float> (maxParticleEnergy), 0.f, 1.f);
	}

	energyRatio = truncateDecimal<float>(energyRatio,6);

	p->lastPos += p->speed;
	p->lastPos.x = truncateDecimal<float>(p->lastPos.x,6);
	p->lastPos.y = truncateDecimal<float>(p->lastPos.y,6);
	p->lastPos.z = truncateDecimal<float>(p->lastPos.z,6);

	p->pos += p->speed;
	p->pos.x = truncateDecimal<float>(p->pos.x,6);
	p->pos.y = truncateDecimal<float>(p->pos.y,6);
	p->pos.z = truncateDecimal<float>(p->pos.z,6);

	if(fixed) {
		p->lastPos += fixedAddition;
		p->lastPos.x = truncateDecimal<float>(p->lastPos.x,6);
		p->lastPos.y = truncateDecimal<float>(p->lastPos.y,6);
		p->lastPos.z = truncateDecimal<float>(p->lastPos.z,6);

		p->pos += fixedAddition;
		p->pos.x = truncateDecimal<float>(p->pos.x,6);
		p->pos.y = truncateDecimal<float>(p->pos.y,6);
		p->pos.z = truncateDecimal<float>(p->pos.z,6);
	}
	p->speed += p->accel;
	p->speed += p->speedUpConstant;
	p->speed=p->speed*(1+p->speedUpRelative);
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);

	p->color= color * energyRatio + colorNoEnergy * (1.0f - energyRatio);
	if(isDaylightAffected==true) {
		p->color.x=p->color.x*lightColor.x;
		p->color.y=p->color.y*lightColor.y;
		p->color.z=p->color.z*lightColor.z;
	}
	p->size= particleSize * energyRatio + sizeNoEnergy * (1.0f - energyRatio);
	p->size = truncateDecimal<float>(p->size,6);

	if(state == ParticleSystem::sFade || staticParticleCount < 1){
		p->energy--;
	}
	else{
		if(maxParticleEnergy > 2){
			if(energyUp){
				p->energy++;
			}
			else{
				p->energy--;
			}

			if(p->energy == 1){
				energyUp= true;
			}
			if(p->energy == maxParticleEnergy){
				energyUp= false;
			}
		}
	}
}

// ================= SET PARAMS ====================

void UnitParticleSystem::setWind(float windAngle, float windSpeed){
#ifdef USE_STREFLOP
	this->windSpeed.x= streflop::sinf(static_cast<streflop::Simple>(degToRad(windAngle)))*windSpeed;
	this->windSpeed.y= 0.0f;
	this->windSpeed.z= streflop::cosf(static_cast<streflop::Simple>(degToRad(windAngle)))*windSpeed;
#else
	this->windSpeed.x= sinf(degToRad(windAngle)) * windSpeed;
	this->windSpeed.y= 0.0f;
	this->windSpeed.z= cosf(degToRad(windAngle)) * windSpeed;
#endif

	this->windSpeed.x = truncateDecimal<float>(this->windSpeed.x,6);
	this->windSpeed.y = truncateDecimal<float>(this->windSpeed.y,6);
	this->windSpeed.z = truncateDecimal<float>(this->windSpeed.z,6);
}

void UnitParticleSystem::saveGame(XmlNode *rootNode) {
	std::map<string,string> mapTagReplacements;
	XmlNode *unitParticleSystemNode = rootNode->addChild("UnitParticleSystem");

	GameParticleSystem::saveGame(unitParticleSystemNode);

//	float radius;
	unitParticleSystemNode->addAttribute("radius",floatToStr(radius,6), mapTagReplacements);
//	float minRadius;
	unitParticleSystemNode->addAttribute("minRadius",floatToStr(minRadius,6), mapTagReplacements);
//	Vec3f windSpeed;
	unitParticleSystemNode->addAttribute("windSpeed",windSpeed.getString(), mapTagReplacements);
//	Vec3f cRotation;
	unitParticleSystemNode->addAttribute("cRotation",cRotation.getString(), mapTagReplacements);
//	Vec3f fixedAddition;
	unitParticleSystemNode->addAttribute("fixedAddition",fixedAddition.getString(), mapTagReplacements);
//  Vec3f oldPosition;
	unitParticleSystemNode->addAttribute("oldPosition",oldPosition.getString(), mapTagReplacements);
//  Vec3f meshPos;
	unitParticleSystemNode->addAttribute("meshPos",meshPos.getString(), mapTagReplacements);
//  string meshName;
	unitParticleSystemNode->addAttribute("meshName",meshName, mapTagReplacements);
//  bool energyUp;
	unitParticleSystemNode->addAttribute("energyUp",intToStr(energyUp), mapTagReplacements);
//	float startTime;
	unitParticleSystemNode->addAttribute("startTime",floatToStr(startTime,6), mapTagReplacements);
//	float endTime;
	unitParticleSystemNode->addAttribute("endTime",floatToStr(endTime,6), mapTagReplacements);
//	bool relative;
	unitParticleSystemNode->addAttribute("relative",intToStr(relative), mapTagReplacements);
//	bool relativeDirection;
	unitParticleSystemNode->addAttribute("relativeDirection",intToStr(relativeDirection), mapTagReplacements);
//  bool fixed;
	unitParticleSystemNode->addAttribute("fixed",intToStr(fixed), mapTagReplacements);
//	Shape shape;
	unitParticleSystemNode->addAttribute("shape",intToStr(shape), mapTagReplacements);
//	float angle;
	unitParticleSystemNode->addAttribute("angle",floatToStr(angle,6), mapTagReplacements);
//	float sizeNoEnergy;
	unitParticleSystemNode->addAttribute("sizeNoEnergy",floatToStr(sizeNoEnergy,6), mapTagReplacements);
//	float gravity;
	unitParticleSystemNode->addAttribute("gravity",floatToStr(gravity,6), mapTagReplacements);
//	float rotation;
	unitParticleSystemNode->addAttribute("rotation",floatToStr(rotation,6), mapTagReplacements);
//	bool isVisibleAtNight;
	unitParticleSystemNode->addAttribute("isVisibleAtNight",intToStr(isVisibleAtNight), mapTagReplacements);
//	bool isVisibleAtDay;
	unitParticleSystemNode->addAttribute("isVisibleAtDay",intToStr(isVisibleAtDay), mapTagReplacements);
//	bool isDaylightAffected;
	unitParticleSystemNode->addAttribute("isDaylightAffected",intToStr(isDaylightAffected), mapTagReplacements);
//	bool radiusBasedStartenergy;
	unitParticleSystemNode->addAttribute("radiusBasedStartenergy",intToStr(radiusBasedStartenergy), mapTagReplacements);
//	int staticParticleCount;
	unitParticleSystemNode->addAttribute("staticParticleCount",intToStr(staticParticleCount), mapTagReplacements);
//	int delay;
	unitParticleSystemNode->addAttribute("delay",intToStr(delay), mapTagReplacements);
//	int lifetime;
	unitParticleSystemNode->addAttribute("lifetime",intToStr(lifetime), mapTagReplacements);
//	float emissionRateFade;
	unitParticleSystemNode->addAttribute("emissionRateFade",floatToStr(emissionRateFade,6), mapTagReplacements);
//	GameParticleSystem* parent;
	//if(parent != NULL) {
	//	parent->saveGame(unitParticleSystemNode);
	//}
}

void UnitParticleSystem::loadGame(const XmlNode *rootNode) {
	const XmlNode *unitParticleSystemNode = rootNode;

	//printf("Load Smoke particle (UnitParticleSystem)\n");
	GameParticleSystem::loadGame(unitParticleSystemNode);

//	float radius;
	radius = unitParticleSystemNode->getAttribute("radius")->getFloatValue();
//	float minRadius;
	minRadius = unitParticleSystemNode->getAttribute("minRadius")->getFloatValue();
//	Vec3f windSpeed;
	windSpeed = Vec3f::strToVec3(unitParticleSystemNode->getAttribute("windSpeed")->getValue());
//	Vec3f cRotation;
	cRotation = Vec3f::strToVec3(unitParticleSystemNode->getAttribute("cRotation")->getValue());
//	Vec3f fixedAddition;
	fixedAddition = Vec3f::strToVec3(unitParticleSystemNode->getAttribute("fixedAddition")->getValue());
//    Vec3f oldPosition;
	oldPosition = Vec3f::strToVec3(unitParticleSystemNode->getAttribute("oldPosition")->getValue());
	//    Vec3f meshPos;
	if(unitParticleSystemNode->hasAttribute("meshPos")){
		meshPos = Vec3f::strToVec3(unitParticleSystemNode->getAttribute("meshPos")->getValue());
	}
	//    Vec3f meshName;
	if(unitParticleSystemNode->hasAttribute("meshName")){
		meshName = unitParticleSystemNode->getAttribute("meshName")->getValue();
	}
//    bool energyUp;
	energyUp = unitParticleSystemNode->getAttribute("energyUp")->getIntValue() != 0;
//	float startTime;
	startTime = unitParticleSystemNode->getAttribute("startTime")->getFloatValue();
//	float endTime;
	endTime = unitParticleSystemNode->getAttribute("endTime")->getFloatValue();
//	bool relative;
	relative = unitParticleSystemNode->getAttribute("relative")->getIntValue() != 0;
//	bool relativeDirection;
	relativeDirection = unitParticleSystemNode->getAttribute("relativeDirection")->getIntValue() != 0;
//    bool fixed;
	fixed = unitParticleSystemNode->getAttribute("fixed")->getIntValue() != 0;
//	Shape shape;
	shape = static_cast<Shape>(unitParticleSystemNode->getAttribute("shape")->getIntValue());
//	float angle;
	angle = unitParticleSystemNode->getAttribute("angle")->getFloatValue();
//	float sizeNoEnergy;
	sizeNoEnergy = unitParticleSystemNode->getAttribute("sizeNoEnergy")->getFloatValue();
//	float gravity;
	gravity = unitParticleSystemNode->getAttribute("gravity")->getFloatValue();
//	float rotation;
	rotation = unitParticleSystemNode->getAttribute("rotation")->getFloatValue();
//	bool isVisibleAtNight;
	isVisibleAtNight = unitParticleSystemNode->getAttribute("isVisibleAtNight")->getIntValue() != 0;
//	bool isVisibleAtDay;
	isVisibleAtDay = unitParticleSystemNode->getAttribute("isVisibleAtDay")->getIntValue() != 0;
//	bool isDaylightAffected;
	isDaylightAffected = unitParticleSystemNode->getAttribute("isDaylightAffected")->getIntValue() != 0;
//	bool radiusBasedStartenergy;
	radiusBasedStartenergy = unitParticleSystemNode->getAttribute("radiusBasedStartenergy")->getIntValue() != 0;
//	int staticParticleCount;
	staticParticleCount = unitParticleSystemNode->getAttribute("staticParticleCount")->getIntValue();
//	int delay;
	delay = unitParticleSystemNode->getAttribute("delay")->getIntValue();
//	int lifetime;
	lifetime = unitParticleSystemNode->getAttribute("lifetime")->getIntValue();
//	float emissionRateFade;
	emissionRateFade = unitParticleSystemNode->getAttribute("emissionRateFade")->getFloatValue();
//	GameParticleSystem* parent;
	parent = NULL;
	//if(parent != NULL) {
	//	parent->saveGame(unitParticleSystemNode);
	//}

	//if(unitParticleSystemNode->hasChild("GameParticleSystem") == true) {
//	void GameParticleSystem::saveGame(XmlNode *rootNode)
//		std::map<string,string> mapTagReplacements;
//		XmlNode *gameParticleSystemNode = rootNode->addChild("GameParticleSystem");
		//XmlNode *gameParticleSystemNode = unitParticleSystemNode->getChild("GameParticleSystem");
		//!!!
	//}
}

Checksum UnitParticleSystem::getCRC() {
	Checksum crcForParticleSystem = GameParticleSystem::getCRC();

	return crcForParticleSystem;
}

string UnitParticleSystem::toString() const {
	string result = GameParticleSystem::toString();

	result += "\nUnitParticleSystem ";
	result += "\nradius = " + floatToStr(radius);
	result += "\nminRadius = " + floatToStr(minRadius);
	result += "\nwindSpeed = " + windSpeed.getString();
	result += "\ncRotation = " + cRotation.getString();
	result += "\nfixedAddition = " + fixedAddition.getString();
    result += "\noldPosition = " + oldPosition.getString();
    result += "\nenergyUp = " + intToStr(energyUp);
    result += "\nstartTime = " + floatToStr(startTime);
    result += "\nendTime = " + floatToStr(endTime);
	result += "\nrelative = " + intToStr(relative);
	result += "\nrelativeDirection = " + intToStr(relativeDirection);
    result += "\nfixed = " + intToStr(fixed);
	result += "\nshape = " + intToStr(shape);

	result += "\nangle = " + floatToStr(angle);
	result += "\nsizeNoEnergy = " + floatToStr(sizeNoEnergy);
	result += "\ngravity = " + floatToStr(gravity);
	result += "\nrotation = " + floatToStr(rotation);
	result += "\nisVisibleAtNight = " + intToStr(isVisibleAtNight);
	result += "\nisVisibleAtDay = " + intToStr(isVisibleAtDay);
	result += "\nisDaylightAffected = " + intToStr(isDaylightAffected);
	result += "\nradiusBasedStartenergy = " + intToStr(radiusBasedStartenergy);
	result += "\nstaticParticleCount = " + intToStr(staticParticleCount);
	result += "\ndelay = " + intToStr(delay);
	result += "\nlifetime = " + intToStr(lifetime);
	result += "\nemissionRateFade = " + floatToStr(emissionRateFade);
	//GameParticleSystem* parent;

	return result;
}

// ===========================================================================
//  RainParticleSystem
// ===========================================================================
RainParticleSystem::RainParticleSystem(int particleCount) :
	ParticleSystem(particleCount){
	setWind(0.0f, 0.0f);
	setRadius(20.0f);

	setEmissionRate(25.0f);
	setParticleSize(3.0f);
	setColor(Vec4f(0.5f, 0.5f, 0.5f, 0.3f));
	setSpeed(0.2f);
}

void RainParticleSystem::render(ParticleRenderer *pr, ModelRenderer *mr){
	pr->renderSystemLineAlpha(this);
}

void RainParticleSystem::initParticle(Particle *p, int particleIndex){
	ParticleSystem::initParticle(p, particleIndex);

	float x= random.randRange(-radius, radius);
	float y= random.randRange(-radius, radius);

	p->color= color;
	p->energy= 10000;
	p->pos= Vec3f(pos.x + x, pos.y, pos.z + y);
	p->pos.x = truncateDecimal<float>(p->pos.x,6);
	p->pos.y = truncateDecimal<float>(p->pos.y,6);
	p->pos.z = truncateDecimal<float>(p->pos.z,6);

	p->lastPos= p->pos;
	p->speed= Vec3f(random.randRange(-speed / 10, speed / 10), -speed,
			random.randRange(-speed / 10, speed / 10)) + windSpeed;
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);
}

bool RainParticleSystem::deathTest(Particle *p){
	return p->pos.y < 0;
}

void RainParticleSystem::setRadius(float radius) {
	this->radius= radius;
}

void RainParticleSystem::setWind(float windAngle, float windSpeed){
#ifdef USE_STREFLOP
	this->windSpeed.x= streflop::sinf(static_cast<streflop::Simple>(degToRad(windAngle)))*windSpeed;
	this->windSpeed.y= 0.0f;
	this->windSpeed.z= streflop::cosf(static_cast<streflop::Simple>(degToRad(windAngle)))*windSpeed;
#else
	this->windSpeed.x= sinf(degToRad(windAngle)) * windSpeed;
	this->windSpeed.y= 0.0f;
	this->windSpeed.z= cosf(degToRad(windAngle)) * windSpeed;
#endif

	this->windSpeed.x = truncateDecimal<float>(this->windSpeed.x,6);
	this->windSpeed.y = truncateDecimal<float>(this->windSpeed.y,6);
	this->windSpeed.z = truncateDecimal<float>(this->windSpeed.z,6);
}

Checksum RainParticleSystem::getCRC() {
	Checksum crcForParticleSystem = ParticleSystem::getCRC();

	return crcForParticleSystem;
}

string RainParticleSystem::toString() const {
	string result = ParticleSystem::toString();

	result += "\nRainParticleSystem ";
	result += "\nwindSpeed = " + windSpeed.getString();
	result += "\nradius = " + floatToStr(radius);

	return result;
}

// ===========================================================================
//  SnowParticleSystem
// ===========================================================================

SnowParticleSystem::SnowParticleSystem(int particleCount) :
	ParticleSystem(particleCount){
	setWind(0.0f, 0.0f);
	setRadius(30.0f);

	setEmissionRate(2.0f);
	setParticleSize(0.2f);
	setColor(Vec4f(0.8f, 0.8f, 0.8f, 0.8f));
	setSpeed(0.05f);
}

void SnowParticleSystem::initParticle(Particle *p, int particleIndex){

	ParticleSystem::initParticle(p, particleIndex);

	float x= random.randRange(-radius, radius);
	float y= random.randRange(-radius, radius);

	p->color= color;
	p->energy= 10000;
	p->pos= Vec3f(pos.x + x, pos.y, pos.z + y);
	p->pos.x = truncateDecimal<float>(p->pos.x,6);
	p->pos.y = truncateDecimal<float>(p->pos.y,6);
	p->pos.z = truncateDecimal<float>(p->pos.z,6);

	p->lastPos= p->pos;
	p->speed= Vec3f(0.0f, -speed, 0.0f) + windSpeed;
	p->speed.x+= random.randRange(-0.005f, 0.005f);
	p->speed.y+= random.randRange(-0.005f, 0.005f);

	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);
}

bool SnowParticleSystem::deathTest(Particle *p){
	return p->pos.y < 0;
}

void SnowParticleSystem::setRadius(float radius){
	this->radius= radius;
}

void SnowParticleSystem::setWind(float windAngle, float windSpeed){
#ifdef USE_STREFLOP
	this->windSpeed.x= streflop::sinf(static_cast<streflop::Simple>(degToRad(windAngle)))*windSpeed;
	this->windSpeed.y= 0.0f;
	this->windSpeed.z= streflop::cosf(static_cast<streflop::Simple>(degToRad(windAngle)))*windSpeed;
#else
	this->windSpeed.x= sinf(degToRad(windAngle)) * windSpeed;
	this->windSpeed.y= 0.0f;
	this->windSpeed.z= cosf(degToRad(windAngle)) * windSpeed;
#endif

	this->windSpeed.x = truncateDecimal<float>(this->windSpeed.x,6);
	this->windSpeed.y = truncateDecimal<float>(this->windSpeed.y,6);
	this->windSpeed.z = truncateDecimal<float>(this->windSpeed.z,6);

}

Checksum SnowParticleSystem::getCRC() {
	Checksum crcForParticleSystem = ParticleSystem::getCRC();

	return crcForParticleSystem;
}

string SnowParticleSystem::toString() const {
	string result = ParticleSystem::toString();

	result += "\nSnowParticleSystem ";
	result += "\nwindSpeed = " + windSpeed.getString();
	result += "\nradius = " + floatToStr(radius);

	return result;
}

// ===========================================================================
//  AttackParticleSystem
// ===========================================================================

AttackParticleSystem::AttackParticleSystem(int particleCount) :
	GameParticleSystem(particleCount){
	primitive= pQuad;
	gravity= 0.0f;
	sizeNoEnergy = 0.0;
}

void AttackParticleSystem::saveGame(XmlNode *rootNode) {
	std::map<string,string> mapTagReplacements;
	XmlNode *attackParticleSystemNode = rootNode->addChild("AttackParticleSystem");

	GameParticleSystem::saveGame(attackParticleSystemNode);

//	float sizeNoEnergy;
	attackParticleSystemNode->addAttribute("sizeNoEnergy",floatToStr(sizeNoEnergy,6), mapTagReplacements);
//	float gravity;
	attackParticleSystemNode->addAttribute("gravity",floatToStr(gravity,6), mapTagReplacements);

}
void AttackParticleSystem::loadGame(const XmlNode *rootNode) {
	const XmlNode *attackParticleSystemNode = rootNode;

	GameParticleSystem::loadGame(attackParticleSystemNode);

	//	float sizeNoEnergy;
	sizeNoEnergy = attackParticleSystemNode->getAttribute("sizeNoEnergy")->getFloatValue();
	//	float gravity;
	gravity = attackParticleSystemNode->getAttribute("gravity")->getFloatValue();
}

Checksum AttackParticleSystem::getCRC() {
	Checksum crcForParticleSystem = GameParticleSystem::getCRC();

	return crcForParticleSystem;
}

string AttackParticleSystem::toString() const {
	string result = GameParticleSystem::toString();

	result += "\nAttackParticleSystem ";
	result += "\nsizeNoEnergy = " + floatToStr(sizeNoEnergy);
	result += "\ngravity = " + floatToStr(gravity);

	return result;
}

// ===========================================================================
//  ProjectileParticleSystem
// ===========================================================================

ProjectileParticleSystem::ProjectileParticleSystem(int particleCount) :
	AttackParticleSystem(particleCount){
	setEmissionRate(20.0f);
	setColor(Vec4f(1.0f, 0.3f, 0.0f, 0.5f));
	setMaxParticleEnergy(100);
	setVarParticleEnergy(50);
	setParticleSize(0.4f);
	setSpeed(0.14f);

	trajectory= tLinear;

	trajectorySpeed= 1.0f;
	trajectoryScale= 1.0f;
	trajectoryFrequency= 1.0f;
	modelCycle=0.0f;

	nextParticleSystem= NULL;
	arriveDestinationDistance = 0.0f;
	//printf("#aXX trajectorySpeed = %f\n",trajectorySpeed);
}

ProjectileParticleSystem::~ProjectileParticleSystem(){
	if(nextParticleSystem != NULL){
		nextParticleSystem->prevParticleSystem= NULL;
	}
}

void ProjectileParticleSystem::link(SplashParticleSystem *particleSystem){
	nextParticleSystem= particleSystem;
	nextParticleSystem->setVisible(false);
	nextParticleSystem->setState(sPause);
	nextParticleSystem->prevParticleSystem= this;
}

void ProjectileParticleSystem::update(){
	//printf("Projectile particle system updating...\n");
	if(state == sPlay){

		lastPos = pos;
		flatPos += zVector * truncateDecimal<float>(trajectorySpeed,6);
		flatPos.x = truncateDecimal<float>(flatPos.x,6);
		flatPos.y = truncateDecimal<float>(flatPos.y,6);
		flatPos.z = truncateDecimal<float>(flatPos.z,6);

		Vec3f targetVector = endPos - startPos;
		targetVector.x = truncateDecimal<float>(targetVector.x,6);
		targetVector.y = truncateDecimal<float>(targetVector.y,6);
		targetVector.z = truncateDecimal<float>(targetVector.z,6);

		Vec3f currentVector = flatPos - startPos;
		currentVector.x = truncateDecimal<float>(currentVector.x,6);
		currentVector.y = truncateDecimal<float>(currentVector.y,6);
		currentVector.z = truncateDecimal<float>(currentVector.z,6);

		// ratio
		float relative= clamp(currentVector.length() / targetVector.length(), 0.0f, 1.0f);
		relative = truncateDecimal<float>(relative,6);

		float absolute = relative;
		setTween(relative,absolute);

		// trajectory
		switch(trajectory) {
			case tLinear: {
				pos= flatPos;
				}
				break;

			case tParabolic: {
				float scaledT   = truncateDecimal<float>(2.0f * (relative - 0.5f),6);
				float paraboleY = truncateDecimal<float>((1.0f - scaledT * scaledT) * trajectoryScale,6);

				pos= flatPos;
				pos.y += paraboleY;
				pos.y = truncateDecimal<float>(pos.y,6);
				}
				break;

			case tSpiral: {
				pos= flatPos;
#ifdef USE_STREFLOP
				pos+= xVector * streflop::cos(static_cast<streflop::Simple>(relative * trajectoryFrequency * targetVector.length())) * trajectoryScale;
				pos.x = truncateDecimal<float>(pos.x,6);
				pos.y = truncateDecimal<float>(pos.y,6);
				pos.z = truncateDecimal<float>(pos.z,6);

				pos+= yVector * streflop::sin(static_cast<streflop::Simple>(relative * trajectoryFrequency * targetVector.length())) * trajectoryScale;
#else
				pos+= xVector * cos(relative * trajectoryFrequency * targetVector.length()) * trajectoryScale;
				pos.x = truncateDecimal<float>(pos.x,6);
				pos.y = truncateDecimal<float>(pos.y,6);
				pos.z = truncateDecimal<float>(pos.z,6);

				pos+= yVector * sin(relative * trajectoryFrequency * targetVector.length()) * trajectoryScale;
#endif
				pos.x = truncateDecimal<float>(pos.x,6);
				pos.y = truncateDecimal<float>(pos.y,6);
				pos.z = truncateDecimal<float>(pos.z,6);
			}
				break;

			default:
				assert(false);
				break;
		}

		direction= (pos - lastPos);
		direction.x = truncateDecimal<float>(direction.x,6);
		direction.y = truncateDecimal<float>(direction.y,6);
		direction.z = truncateDecimal<float>(direction.z,6);

		direction.normalize();
		direction.x = truncateDecimal<float>(direction.x,6);
		direction.y = truncateDecimal<float>(direction.y,6);
		direction.z = truncateDecimal<float>(direction.z,6);

		// trigger update of child particles
		positionChildren(); 
		rotateChildren();

		//arrive destination
		arriveDestinationDistance = truncateDecimal<float>(flatPos.dist(endPos),6);

		if(this->particleOwner != NULL) {
			char szBuf[8096]="";
			snprintf(szBuf,8095,"LINE: %d arriveDestinationDistance = %f",__LINE__,arriveDestinationDistance);
			this->particleOwner->logParticleInfo(szBuf);
		}

		if(arriveDestinationDistance < 0.5f || relative >= 1) {
			fade();
			model= NULL;

			if(particleObserver != NULL){
				particleObserver->update(this);
				particleObserver=NULL;
			}

			if(nextParticleSystem != NULL){
				nextParticleSystem->setVisible(getVisible());
				nextParticleSystem->setState(sPlay);
				nextParticleSystem->setPos(endPos);
			}
		}
	}

	ParticleSystem::update();
}

void ProjectileParticleSystem::rotateChildren() {
	//### only on horizontal plane :(
#ifdef USE_STREFLOP
	float rotation = truncateDecimal<float>(streflop::atan2(static_cast<streflop::Simple>(direction.x), static_cast<streflop::Simple>(direction.z)),6);
#else
	float rotation = truncateDecimal<float>(atan2(direction.x, direction.z),6);
#endif	
	rotation = truncateDecimal<float>(radToDeg(rotation),6);
	for(Children::iterator it = children.begin(); it != children.end(); ++it)
		(*it)->setRotation(rotation);
}

void ProjectileParticleSystem::initParticle(Particle *p, int particleIndex){

	ParticleSystem::initParticle(p, particleIndex);

	float t= static_cast<float> (particleIndex) / emissionRate;
	t = truncateDecimal<float>(t,6);

	p->pos= pos + (lastPos - pos) * t;
	p->pos.x = truncateDecimal<float>(p->pos.x,6);
	p->pos.y = truncateDecimal<float>(p->pos.y,6);
	p->pos.z = truncateDecimal<float>(p->pos.z,6);

	p->lastPos= lastPos;
	p->speed= Vec3f(random.randRange(-0.1f, 0.1f), random.randRange(-0.1f, 0.1f),
			random.randRange(-0.1f, 0.1f)) * speed;
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);

	p->accel= Vec3f(0.0f, -gravity, 0.0f);
	p->accel.x = truncateDecimal<float>(p->accel.x,6);
	p->accel.y = truncateDecimal<float>(p->accel.y,6);
	p->accel.z = truncateDecimal<float>(p->accel.z,6);

	updateParticle(p);
}

void ProjectileParticleSystem::updateParticle(Particle *p){
	float energyRatio= clamp(static_cast<float> (p->energy) / maxParticleEnergy, 0.f, 1.f);
	energyRatio = truncateDecimal<float>(energyRatio,6);

	p->lastPos += p->speed;
	p->lastPos.x = truncateDecimal<float>(p->lastPos.x,6);
	p->lastPos.y = truncateDecimal<float>(p->lastPos.y,6);
	p->lastPos.z = truncateDecimal<float>(p->lastPos.z,6);

	p->pos += p->speed;
	p->pos.x = truncateDecimal<float>(p->pos.x,6);
	p->pos.y = truncateDecimal<float>(p->pos.y,6);
	p->pos.z = truncateDecimal<float>(p->pos.z,6);

	p->speed += p->accel;
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);

	p->color= color * energyRatio + colorNoEnergy * (1.0f - energyRatio);
	p->size = particleSize * energyRatio + sizeNoEnergy * (1.0f - energyRatio);
	p->size = truncateDecimal<float>(p->size,6);
	p->energy--;
}

void ProjectileParticleSystem::setPath(Vec3f startPos, Vec3f endPos) {
	startPos.x = truncateDecimal<float>(startPos.x,6);
	startPos.y = truncateDecimal<float>(startPos.y,6);
	startPos.z = truncateDecimal<float>(startPos.z,6);

	endPos.x = truncateDecimal<float>(endPos.x,6);
	endPos.y = truncateDecimal<float>(endPos.y,6);
	endPos.z = truncateDecimal<float>(endPos.z,6);

	//compute axis
	zVector= endPos - startPos;
	zVector.x = truncateDecimal<float>(zVector.x,6);
	zVector.y = truncateDecimal<float>(zVector.y,6);
	zVector.z = truncateDecimal<float>(zVector.z,6);

	zVector.normalize();
	zVector.x = truncateDecimal<float>(zVector.x,6);
	zVector.y = truncateDecimal<float>(zVector.y,6);
	zVector.z = truncateDecimal<float>(zVector.z,6);

	yVector= Vec3f(0.0f, 1.0f, 0.0f);
	xVector= zVector.cross(yVector);
	xVector.x = truncateDecimal<float>(xVector.x,6);
	xVector.y = truncateDecimal<float>(xVector.y,6);
	xVector.z = truncateDecimal<float>(xVector.z,6);

	//apply offset
	startPos += xVector * offset.x;
	startPos.x = truncateDecimal<float>(startPos.x,6);
	startPos.y = truncateDecimal<float>(startPos.y,6);
	startPos.z = truncateDecimal<float>(startPos.z,6);

	startPos+= yVector * offset.y;
	startPos.x = truncateDecimal<float>(startPos.x,6);
	startPos.y = truncateDecimal<float>(startPos.y,6);
	startPos.z = truncateDecimal<float>(startPos.z,6);

	startPos+= zVector * offset.z;
	startPos.x = truncateDecimal<float>(startPos.x,6);
	startPos.y = truncateDecimal<float>(startPos.y,6);
	startPos.z = truncateDecimal<float>(startPos.z,6);

	pos= startPos;
	lastPos= startPos;
	flatPos= startPos;

	//recompute axis
	zVector= endPos - startPos;
	zVector.x = truncateDecimal<float>(zVector.x,6);
	zVector.y = truncateDecimal<float>(zVector.y,6);
	zVector.z = truncateDecimal<float>(zVector.z,6);

	zVector.normalize();
	zVector.x = truncateDecimal<float>(zVector.x,6);
	zVector.y = truncateDecimal<float>(zVector.y,6);
	zVector.z = truncateDecimal<float>(zVector.z,6);

	yVector= Vec3f(0.0f, 1.0f, 0.0f);
	xVector= zVector.cross(yVector);
	xVector.x = truncateDecimal<float>(xVector.x,6);
	xVector.y = truncateDecimal<float>(xVector.y,6);
	xVector.z = truncateDecimal<float>(xVector.z,6);

	// set members
	this->startPos= startPos;
	this->endPos= endPos;
	
	// direction
	direction = (endPos - lastPos);
	direction.x = truncateDecimal<float>(direction.x,6);
	direction.y = truncateDecimal<float>(direction.y,6);
	direction.z = truncateDecimal<float>(direction.z,6);

	direction.normalize();
	direction.x = truncateDecimal<float>(direction.x,6);
	direction.y = truncateDecimal<float>(direction.y,6);
	direction.z = truncateDecimal<float>(direction.z,6);

	rotateChildren();
}

ProjectileParticleSystem::Trajectory ProjectileParticleSystem::strToTrajectory(const string &str){
	if(str == "linear"){
		return tLinear;
	}
	else if(str == "parabolic"){
		return tParabolic;
	}
	else if(str == "spiral"){
		return tSpiral;
	}
	else{
		throw megaglest_runtime_error("Unknown particle system trajectory: " + str);
	}
}

void ProjectileParticleSystem::saveGame(XmlNode *rootNode) {
	std::map<string,string> mapTagReplacements;
	XmlNode *projectileParticleSystemNode = rootNode->addChild("ProjectileParticleSystem");

	AttackParticleSystem::saveGame(projectileParticleSystemNode);

//	SplashParticleSystem *nextParticleSystem;
	if(nextParticleSystem != NULL) {
		nextParticleSystem->saveGame(projectileParticleSystemNode);
	}
//	Vec3f lastPos;
	projectileParticleSystemNode->addAttribute("lastPos",lastPos.getString(), mapTagReplacements);
//	Vec3f startPos;
	projectileParticleSystemNode->addAttribute("startPos",startPos.getString(), mapTagReplacements);
//	Vec3f endPos;
	projectileParticleSystemNode->addAttribute("endPos",endPos.getString(), mapTagReplacements);
//	Vec3f flatPos;
	projectileParticleSystemNode->addAttribute("flatPos",flatPos.getString(), mapTagReplacements);
//
//	Vec3f xVector;
	projectileParticleSystemNode->addAttribute("xVector",xVector.getString(), mapTagReplacements);
//	Vec3f yVector;
	projectileParticleSystemNode->addAttribute("yVector",yVector.getString(), mapTagReplacements);
//	Vec3f zVector;
	projectileParticleSystemNode->addAttribute("zVector",zVector.getString(), mapTagReplacements);
//	Trajectory trajectory;
	projectileParticleSystemNode->addAttribute("trajectory",intToStr(trajectory), mapTagReplacements);
//	float trajectorySpeed;
	projectileParticleSystemNode->addAttribute("trajectorySpeed",floatToStr(trajectorySpeed,6), mapTagReplacements);
//	//parabolic
//	float trajectoryScale;
	projectileParticleSystemNode->addAttribute("trajectoryScale",floatToStr(trajectoryScale,6), mapTagReplacements);
//	float trajectoryFrequency;
	projectileParticleSystemNode->addAttribute("trajectoryFrequency",floatToStr(trajectoryFrequency,6), mapTagReplacements);
}

void ProjectileParticleSystem::loadGame(const XmlNode *rootNode) {
	const XmlNode *projectileParticleSystemNode = rootNode;

	AttackParticleSystem::loadGame(projectileParticleSystemNode);

	//	SplashParticleSystem *nextParticleSystem;
//	if(nextParticleSystem != NULL) {
//		nextParticleSystem->saveGame(projectileParticleSystemNode);
//	}
	if(projectileParticleSystemNode->hasChild("SplashParticleSystem") == true) {
		XmlNode *splashParticleSystemNode = projectileParticleSystemNode->getChild("SplashParticleSystem");
		nextParticleSystem = new SplashParticleSystem();
		nextParticleSystem->setParticleOwner(this->getParticleOwner());
		nextParticleSystem->loadGame(splashParticleSystemNode);
	}
	//	Vec3f lastPos;
	lastPos = Vec3f::strToVec3(projectileParticleSystemNode->getAttribute("lastPos")->getValue());
	//	Vec3f startPos;
	startPos = Vec3f::strToVec3(projectileParticleSystemNode->getAttribute("startPos")->getValue());
	//	Vec3f endPos;
	endPos = Vec3f::strToVec3(projectileParticleSystemNode->getAttribute("endPos")->getValue());
	//	Vec3f flatPos;
	flatPos = Vec3f::strToVec3(projectileParticleSystemNode->getAttribute("flatPos")->getValue());
	//
	//	Vec3f xVector;
	xVector = Vec3f::strToVec3(projectileParticleSystemNode->getAttribute("xVector")->getValue());
	//	Vec3f yVector;
	yVector = Vec3f::strToVec3(projectileParticleSystemNode->getAttribute("yVector")->getValue());
	//	Vec3f zVector;
	zVector = Vec3f::strToVec3(projectileParticleSystemNode->getAttribute("zVector")->getValue());
	//	Trajectory trajectory;
	trajectory = static_cast<Trajectory>(projectileParticleSystemNode->getAttribute("trajectory")->getIntValue());
	//	float trajectorySpeed;
	trajectorySpeed = projectileParticleSystemNode->getAttribute("trajectorySpeed")->getFloatValue();
	//	//parabolic
	//	float trajectoryScale;
	trajectoryScale = projectileParticleSystemNode->getAttribute("trajectoryScale")->getFloatValue();
	//	float trajectoryFrequency;
	trajectoryFrequency = projectileParticleSystemNode->getAttribute("trajectoryFrequency")->getFloatValue();
}

Checksum ProjectileParticleSystem::getCRC() {
	Checksum crcForParticleSystem = AttackParticleSystem::getCRC();

	return crcForParticleSystem;
}

string ProjectileParticleSystem::toString() const {
	string result = AttackParticleSystem::toString();

	result += "\nProjectileParticleSystem ";
	if(nextParticleSystem != NULL) {
		//result += "\nnextParticleSystem = " + nextParticleSystem->toString() + "\n";
		result += "\nnextParticleSystem = NOT NULL\n";
	}
	result += "\nlastPos = " + lastPos.getString();
	result += "\nstartPos = " + startPos.getString();
	result += "\nendPos = " + endPos.getString();
	result += "\nflatPos = " + flatPos.getString();

	result += "\nxVector = " + xVector.getString();
	result += "\nyVector = " + yVector.getString();
	result += "\nzVector = " + zVector.getString();

	result += "\ntrajectory = " + intToStr(trajectory);
	result += "\ntrajectorySpeed = " + floatToStr(trajectorySpeed);
	result += "\ntrajectoryScale = " + floatToStr(trajectoryScale);
	result += "\ntrajectoryFrequency = " + floatToStr(trajectoryFrequency);

	result += "\narriveDestinationDistance = " + floatToStr(arriveDestinationDistance);

	return result;
}

// ===========================================================================
//  SplashParticleSystem
// ===========================================================================

SplashParticleSystem::SplashParticleSystem(int particleCount) :
	AttackParticleSystem(particleCount){
	setColor(Vec4f(1.0f, 0.3f, 0.0f, 0.8f));
	setMaxParticleEnergy(100);
	setVarParticleEnergy(50);
	setParticleSize(1.0f);
	setSpeed(0.003f);

	prevParticleSystem= NULL;

	emissionRateFade= 1.0f;
	verticalSpreadA= 1.0f;
	verticalSpreadB= 0.0f;
	horizontalSpreadA= 1.0f;
	horizontalSpreadB= 0.0f;
	startEmissionRate= 0.0f;
}

SplashParticleSystem::~SplashParticleSystem(){
	if(prevParticleSystem != NULL){
		prevParticleSystem->nextParticleSystem= NULL;
	}
}

void SplashParticleSystem::initParticleSystem() {
	startEmissionRate = emissionRate;
}

void SplashParticleSystem::update() {
	ParticleSystem::update();
	if(state != sPause) {
		emissionRate -= emissionRateFade;
		emissionRate = truncateDecimal<float>(emissionRate,6);

		float t = 1.0f - ((emissionRate + startEmissionRate) / (startEmissionRate * 2.0f));
		t = truncateDecimal<float>(t,6);

		t= clamp(t, 0.0f, 1.0f);
		setTween(t,t);

		if(this->particleOwner != NULL) {
			char szBuf[8096]="";
			snprintf(szBuf,8095,"LINE: %d emissionRate = %f",__LINE__,emissionRate);
			this->particleOwner->logParticleInfo(szBuf);
		}

		if(emissionRate < 0.0f) {//otherwise this system lives forever!
			fade();
		}
	}
}

void SplashParticleSystem::initParticle(Particle *p, int particleIndex){
	p->pos= pos;
	p->lastPos= p->pos;
	p->energy= maxParticleEnergy;
	p->size= particleSize;
	p->color= color;
	p->speedUpRelative= speedUpRelative;

	p->speed= Vec3f(horizontalSpreadA * random.randRange(-1.0f, 1.0f) + horizontalSpreadB, verticalSpreadA
	        * random.randRange(-1.0f, 1.0f) + verticalSpreadB, horizontalSpreadA * random.randRange(-1.0f, 1.0f)
	        + horizontalSpreadB);
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);

	p->speed.normalize();
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);

	p->speed= p->speed * speed;
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);

	p->accel= Vec3f(0.0f, -gravity, 0.0f);
	p->accel.x = truncateDecimal<float>(p->accel.x,6);
	p->accel.y = truncateDecimal<float>(p->accel.y,6);
	p->accel.z = truncateDecimal<float>(p->accel.z,6);

	p->speedUpConstant= Vec3f(speedUpConstant)*p->speed;
}

void SplashParticleSystem::updateParticle(Particle *p){
	float energyRatio= clamp(static_cast<float> (p->energy) / maxParticleEnergy, 0.f, 1.f);

	p->lastPos= p->pos;
	p->pos= p->pos + p->speed;
	p->pos.x = truncateDecimal<float>(p->pos.x,6);
	p->pos.y = truncateDecimal<float>(p->pos.y,6);
	p->pos.z = truncateDecimal<float>(p->pos.z,6);

	p->speed += p->speedUpConstant;
	p->speed=p->speed*(1+p->speedUpRelative);
	p->speed= p->speed + p->accel;
	p->speed.x = truncateDecimal<float>(p->speed.x,6);
	p->speed.y = truncateDecimal<float>(p->speed.y,6);
	p->speed.z = truncateDecimal<float>(p->speed.z,6);

	p->energy--;
	p->color= color * energyRatio + colorNoEnergy * (1.0f - energyRatio);
	p->size= particleSize * energyRatio + sizeNoEnergy * (1.0f - energyRatio);
	p->size = truncateDecimal<float>(p->size,6);
}

void SplashParticleSystem::saveGame(XmlNode *rootNode) {
	std::map<string,string> mapTagReplacements;
	XmlNode *splashParticleSystemNode = rootNode->addChild("SplashParticleSystem");

	AttackParticleSystem::saveGame(splashParticleSystemNode);

//	ProjectileParticleSystem *prevParticleSystem;
	if(prevParticleSystem != NULL) {
		prevParticleSystem->saveGame(splashParticleSystemNode);
	}

//	float emissionRateFade;
	splashParticleSystemNode->addAttribute("emissionRateFade",floatToStr(emissionRateFade,6), mapTagReplacements);
//	float verticalSpreadA;
	splashParticleSystemNode->addAttribute("verticalSpreadA",floatToStr(verticalSpreadA,6), mapTagReplacements);
//	float verticalSpreadB;
	splashParticleSystemNode->addAttribute("verticalSpreadB",floatToStr(verticalSpreadB,6), mapTagReplacements);
//	float horizontalSpreadA;
	splashParticleSystemNode->addAttribute("horizontalSpreadA",floatToStr(horizontalSpreadA,6), mapTagReplacements);
//	float horizontalSpreadB;
	splashParticleSystemNode->addAttribute("horizontalSpreadB",floatToStr(horizontalSpreadB,6), mapTagReplacements);
//
//	float startEmissionRate;
	splashParticleSystemNode->addAttribute("startEmissionRate",floatToStr(startEmissionRate,6), mapTagReplacements);
}

void SplashParticleSystem::loadGame(const XmlNode *rootNode) {
	const XmlNode *splashParticleSystemNode = rootNode;

	AttackParticleSystem::loadGame(splashParticleSystemNode);

	//	ProjectileParticleSystem *prevParticleSystem;
//	if(nextParticleSystem != NULL) {
//		nextParticleSystem->saveGame(projectileParticleSystemNode);
//	}
	if(splashParticleSystemNode->hasChild("ProjectileParticleSystem") == true) {
		XmlNode *projectileParticleSystemNode = splashParticleSystemNode->getChild("ProjectileParticleSystem");
		prevParticleSystem = new ProjectileParticleSystem();
		prevParticleSystem->setParticleOwner(this->getParticleOwner());
		prevParticleSystem->loadGame(projectileParticleSystemNode);
	}

//	float emissionRateFade;
	emissionRateFade = splashParticleSystemNode->getAttribute("emissionRateFade")->getFloatValue();
//	float verticalSpreadA;
	verticalSpreadA = splashParticleSystemNode->getAttribute("verticalSpreadA")->getFloatValue();
//	float verticalSpreadB;
	verticalSpreadB = splashParticleSystemNode->getAttribute("verticalSpreadB")->getFloatValue();
//	float horizontalSpreadA;
	horizontalSpreadA = splashParticleSystemNode->getAttribute("horizontalSpreadA")->getFloatValue();
//	float horizontalSpreadB;
	horizontalSpreadB = splashParticleSystemNode->getAttribute("horizontalSpreadB")->getFloatValue();
//	float startEmissionRate;
	startEmissionRate = splashParticleSystemNode->getAttribute("startEmissionRate")->getFloatValue();
}

Checksum SplashParticleSystem::getCRC() {
	Checksum crcForParticleSystem = AttackParticleSystem::getCRC();

	return crcForParticleSystem;
}

string SplashParticleSystem::toString() const {
	string result = AttackParticleSystem::toString();

	result += "\nSplashParticleSystem ";
	if(prevParticleSystem != NULL) {
		//result += "\nprevParticleSystem = " + prevParticleSystem->toString() + "\n";
		result += "\nprevParticleSystem = NOT NULL\n";
	}

	result += "\nemissionRateFade = " + floatToStr(emissionRateFade);
	result += "\nverticalSpreadA = " + floatToStr(verticalSpreadA);
	result += "\nverticalSpreadB = " + floatToStr(verticalSpreadB);
	result += "\nhorizontalSpreadA = " + floatToStr(horizontalSpreadA);
	result += "\nhorizontalSpreadB = " + floatToStr(horizontalSpreadB);
	result += "\nstartEmissionRate = " + floatToStr(startEmissionRate);

	return result;
}

// ===========================================================================
//  ParticleManager
// ===========================================================================

ParticleManager::ParticleManager() {
}

ParticleManager::~ParticleManager() {
	end();
}

void ParticleManager::render(ParticleRenderer *pr, ModelRenderer *mr) const{
	for(unsigned int i= 0; i < particleSystems.size(); i++){
		ParticleSystem *ps= particleSystems[i];
		if(ps != NULL && ps->getVisible()){
			ps->render(pr, mr);
		}
	}
}

bool ParticleManager::hasActiveParticleSystem(ParticleSystem::ParticleSystemType type) const{
	bool result= false;

	//size_t particleSystemCount= particleSystems.size();
	//int currentParticleCount= 0;

	//vector<ParticleSystem *> cleanupParticleSystemsList;
	for(unsigned int i= 0; i < particleSystems.size(); i++){
		ParticleSystem *ps= particleSystems[i];
		if(ps != NULL){
			//currentParticleCount+= ps->getAliveParticleCount();

			bool showParticle= true;
			if(dynamic_cast<UnitParticleSystem *> (ps) != NULL ||
			   dynamic_cast<FireParticleSystem *> (ps) != NULL) {
				showParticle= ps->getVisible() || (ps->getState() == ParticleSystem::sFade);
			}
			if(showParticle == true){
				//printf("Looking for [%d] current id [%d] i = %d\n",type,ps->getParticleSystemType(),i);

				if(type == ParticleSystem::pst_All || type == ps->getParticleSystemType()){
					//printf("FOUND particle system type match for [%d] current id [%d] i = %d\n",type,ps->getParticleSystemType(),i);
					result= true;
					break;
				}
			}
		}
	}

	return result;
}

void ParticleManager::update(int renderFps){
	Chrono chrono;
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) chrono.start();

	size_t particleSystemCount= particleSystems.size();
	int currentParticleCount= 0;

	vector<ParticleSystem *> cleanupParticleSystemsList;
	for(unsigned int i= 0; i < particleSystems.size(); i++){
		ParticleSystem *ps= particleSystems[i];
		if(ps != NULL) {
			currentParticleCount+= ps->getAliveParticleCount();

			bool showParticle= true;
			if( dynamic_cast<UnitParticleSystem *> (ps) != NULL ||
				dynamic_cast<FireParticleSystem *> (ps) != NULL) {
				showParticle = ps->getVisible() || (ps->getState() == ParticleSystem::sFade);
			}
			if(showParticle == true){
				ps->update();
				if(ps->isEmpty() && ps->getState() == ParticleSystem::sFade) {
					cleanupParticleSystemsList.push_back(ps);
				}
			}
		}
	}
	//particleSystems.remove(NULL);
	cleanupParticleSystems(cleanupParticleSystemsList);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0)
		SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld, particleSystemCount = %d, currentParticleCount = %d\n",__FILE__,__FUNCTION__,__LINE__,chrono.getMillis(),particleSystemCount,currentParticleCount);
}

bool ParticleManager::validateParticleSystemStillExists(ParticleSystem * particleSystem) const{
	int index= findParticleSystems(particleSystem, this->particleSystems);
	return (index >= 0);
}

void ParticleManager::removeParticleSystemsForParticleOwner(ParticleOwner *particleOwner) {
	if(particleOwner != NULL && particleSystems.empty() == false) {
		vector<ParticleSystem *> cleanupParticleSystemsList;

		for(unsigned int index = 0; index < particleSystems.size(); ++index) {
			ParticleSystem *ps= particleSystems[index];
			if(ps != NULL && ps->getParticleOwner() == particleOwner) {
				cleanupParticleSystemsList.push_back(ps);
			}
		}
		if(cleanupParticleSystemsList.empty() == false) {
			cleanupParticleSystems(cleanupParticleSystemsList);
		}
	}
}

int ParticleManager::findParticleSystems(ParticleSystem *psFind, const vector<ParticleSystem *> &particleSystems) const{
	int result= -1;
	for(unsigned int i= 0; i < particleSystems.size(); i++){
		ParticleSystem *ps= particleSystems[i];
		if(ps != NULL && psFind != NULL && psFind == ps){
			result= i;
			break;
		}
	}
	return result;
}

void ParticleManager::cleanupParticleSystems(ParticleSystem *ps) {

	int index= findParticleSystems(ps, this->particleSystems);
	if(ps != NULL && index >= 0) {
//		printf("-- Delete cleanupParticleSystems [%p]\n",ps);
//		static map<void *,int> deleteList;
//		if(deleteList.find(ps) != deleteList.end()) {
//			assert(deleteList.find(ps) == deleteList.end());
//		}
//		deleteList[ps]++;

		// This code causes segfault on game end, no need to fade, just delete
		//if(ps->getState() != ParticleSystem::sFade) {
		//	ps->fade();
		//}

		if(ps != NULL) {
			ps->callParticleOwnerEnd(ps);
		}

		delete ps;
		this->particleSystems.erase(this->particleSystems.begin() + index);
	}
}

void ParticleManager::cleanupParticleSystems(vector<ParticleSystem *> &cleanupParticleSystemsList){
	if(cleanupParticleSystemsList.empty() == false) {
		for(int i= (int)cleanupParticleSystemsList.size()-1; i >= 0; i--) {
			ParticleSystem *ps= cleanupParticleSystemsList[i];
			cleanupParticleSystems(ps);
		}

		cleanupParticleSystemsList.clear();
	}
}

void ParticleManager::cleanupUnitParticleSystems(vector<UnitParticleSystem *> &cleanupParticleSystemsList){
	if(cleanupParticleSystemsList.empty() == false) {
		for(int i= (int)cleanupParticleSystemsList.size()-1; i >= 0; i--) {
			ParticleSystem *ps= cleanupParticleSystemsList[i];
			cleanupParticleSystems(ps);
		}
		cleanupParticleSystemsList.clear();
	}
}

void ParticleManager::manage(ParticleSystem *ps){
	assert((std::find(particleSystems.begin(),particleSystems.end(),ps) == particleSystems.end()) && "particle cannot be added twice");
	particleSystems.push_back(ps);
	for(int i = ps->getChildCount() - 1; i >= 0; i--) {
		manage(ps->getChild(i));
	}
}

void ParticleManager::end(){
	while(particleSystems.empty() == false){
		ParticleSystem *ps = particleSystems.back();

//		printf("-- Delete end() [%p]\n",ps);
//		static map<void *,int> deleteList;
//		if(deleteList.find(ps) != deleteList.end()) {
//			assert(deleteList.find(ps) == deleteList.end());
//		}
//		deleteList[ps]++;

		if(ps != NULL) {
			ps->callParticleOwnerEnd(ps);
		}
		delete ps;
		particleSystems.pop_back();
	}
}

}
}//end namespace
