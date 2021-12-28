/*
Copyright (C) 2021 Sauraen
You may redistribute, create derivative works of, and otherwise copy this
software, subject to the following conditions:
- If the software or modified versions of it are distributed in source form,
  it must retain this copyright notice and list of conditions
- Sauraen must be credited (e.g. in the Special Thanks) of any project using
  this software or modified versions of it

See README.md for more general information about OoT's math conventions and the
design of this patch. Algorithms are modified from:
https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/
http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/Quaternions.pdf

To use: Replace SkelAnime_InterpFrameTable with Patched_InterpFrameTable. The
other functions are used by it and must be accessible to it somewhere. Of
course you can move the Quaternion struct to a header, remove `static inline`,
etc.
*/

typedef struct {
    float w;
    float x;
    float y;
    float z;
} Quaternion;

static inline s16 Fixed_atan2s(float y, float x){
    //atan2 is defined as atan2(y, x). OoT has the arguments backwards, just 
    //like it has them backwards in bcopy and other functions.
    /*
    //This is a test to prove this.
    printf("atan2 %04X %04X %04X %04X",
        Math_Atan2S(0.0f, 1.0f), //should be 0 -> displays 4000
        Math_Atan2S(1.0f, 0.0f), //should be 4000 -> displays 0
        Math_Atan2S(0.0f, -1.0f), //should be 8000 -> displays C000
        Math_Atan2S(-1.0f, 0.0f) //should be C000 -> displays 8000
    );
    */
    return Math_Atan2S(x, y);
}

static inline void Euler2Quat(const Vec3s *r, Quaternion *q){
    float cx = Math_CosS(r->x / 2);
    float sx = Math_SinS(r->x / 2);
    float cy = Math_CosS(r->y / 2);
    float sy = Math_SinS(r->y / 2);
    float cz = Math_CosS(r->z / 2);
    float sz = Math_SinS(r->z / 2);
    q->w = cx * cy * cz + sx * sy * sz;
    q->x = sx * cy * cz - cx * sy * sz;
    q->y = cx * sy * cz + sx * cy * sz;
    q->z = cx * cy * sz - sx * sy * cz;
}

static inline void Quat2Euler(const Quaternion *q, Vec3s *r){
    //Normalize the quaternion
    float mult = q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z;
    if(mult < 0.001f){
        //This can occur when a new morph is started while another morph is
        //ongoing, corrupting the morph table. This check avoids a crash due to
        //divide-by-zero.
        //printf("output quaternion is 0");
        mult = 0.001f;
    }
    //Normally we would divide each component by 1 / sqrt(mult), but the
    //components are only ever used multiplied in pairs, so it becomes 1 / mult
    //and we factor that out. There's also a 2 wherever this ends up being used
    //in the equations below, so that's also factored out here.
    mult = 2.0f / mult;
    float temp = mult * (q->w * q->y - q->x * q->z);
    if(temp >= 1.0f){
        r->y = 0x4000;
    }else if(temp <= -1.0f){
        r->y = 0xC000;
    }else{
        r->x = Fixed_atan2s(mult * (q->w * q->x + q->y * q->z), 1.0f - mult * (q->x * q->x + q->y * q->y));
        r->y = Fixed_atan2s(temp, sqrtf(1.0f - temp * temp));
        r->z = Fixed_atan2s(mult * (q->w * q->z + q->x * q->y), 1.0f - mult * (q->y * q->y + q->z * q->z));
        return;
    }
    //for both of the singularity cases above:
    r->x = Math_Atan2S(q->x, q->w);
    r->z = 0;
}

//This is an optional way to have the new algorithm only applied to SkelAnime
//instances you want it to be, by enabling it before SkelAnime_Update and
//disabling it afterwards. It will use the vanilla algorithm if disabled.
//If you want it enabled globally, you can remove this.
static u8 useSlerp = 0;
void Patched_MorphUseSlerp(u8 enabled){
    useSlerp = enabled;
}

static void Patched_InterpFrameTable(s32 limbCount, Vec3s* dst, Vec3s* start, Vec3s* target, f32 weight) {
    if(weight >= 1.0f){
        bcopy(target, dst, limbCount * sizeof(Vec3s));
        return;
    }
    if(weight <= 0.0f){
        bcopy(start, dst, limbCount * sizeof(Vec3s));
        return;
    }
    for(s32 i=0; i<limbCount; i++, dst++, start++, target++){
        s32 numLargeRot = 0;
        s16 dx = target->x - start->x;
        s16 dy = target->y - start->y;
        s16 dz = target->z - start->z;
        //Try to detect whether it's worth doing the expensive slerp algorithm.
        //If the angles don't change by much, or there is only one axis with a
        //large angle change, the results from the much cheaper vanilla 
        //algorithm are probably okay. However, the slerp algorithm is not
        //*super* expensive, since all the trig functions are done by lookup
        //tables. You might want to consider enabling it always, or at least
        //making the conditions for using the vanilla algorithm more strict.
        //One other thing to check would be not just the size of the change in
        //rotation, but how close the starting or ending rotation is to the
        //0x7FFF/0x8000 boundary, since this is where problems can occur.
        if(dx <= 0xC000 || dx >= 0x4000) ++numLargeRot;
        if(dy <= 0xC000 || dy >= 0x4000) ++numLargeRot;
        if(dz <= 0xC000 || dz >= 0x4000) ++numLargeRot;
        if(numLargeRot >= 2 && useSlerp && i >= 1){
            //i==0 is translation. If you modify the conditions for using the
            //slerp algorithm, make sure not to remove the i >= 1 check, it will
            //be massively wrong and often crash if the slerp is mistakenly
            //applied to the translation.
            //Spherical linear interpolation between quaternions.
            Quaternion qs, qt, qo;
            Euler2Quat(start, &qs);
            Euler2Quat(target, &qt);
            float cosHalfTheta = qs.w * qt.w + qs.x * qt.x + qs.y * qt.y + qs.z * qt.z;
            float wtmult = 1.0f;
            if(cosHalfTheta < 0.0f){
                //Negate one of the quaternions to get the closer rotation solution
                wtmult = -1.0f;
                cosHalfTheta = -cosHalfTheta;
            }
            float ws, wt;
            if(cosHalfTheta > 0.97f){
                //Rotations are very close. We must avoid the divide by zero
                //in the sin below, but since they are close, linear
                //interpolation (with the normalization in Quat2Euler) is good
                //enough.
                ws = 1.0f - weight;
                wt = weight;
            }else{
                //OoT does not have asins, so we use atan2s.
                //It does have a full float asin, but this internally uses the
                //full float atan2.
                float sinHalfTheta = sqrtf(1.0f - cosHalfTheta * cosHalfTheta);
                s16 halfTheta = Fixed_atan2s(sinHalfTheta, cosHalfTheta);
                float rcpSinHalfTheta = 1.0f / sinHalfTheta;
                ws = Math_SinS((1.0f - weight) * halfTheta) * rcpSinHalfTheta;
                wt = Math_SinS(weight * halfTheta) * rcpSinHalfTheta;
            }
            wt *= wtmult;
            qo.w = ws * qs.w + wt * qt.w;
            qo.x = ws * qs.x + wt * qt.x;
            qo.y = ws * qs.y + wt * qt.y;
            qo.z = ws * qs.z + wt * qt.z;
            Quat2Euler(&qo, dst);
        }else{
            dst->x = (s16)(dx * weight) + start->x;
            dst->y = (s16)(dy * weight) + start->y;
            dst->z = (s16)(dz * weight) + start->z;
        }
    }
}
